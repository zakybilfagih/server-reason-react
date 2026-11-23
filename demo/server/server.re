let debug = Sys.getenv_opt("DEMO_ENV") == Some("development");

let serverFunctionHandler =
  DreamRouter.streamFunctionResponse(~debug, ~lookup=FunctionReferences.get);

let getAndPost = (path, handler) =>
  Dream.scope(
    "/",
    [],
    [Dream.get(path, handler), Dream.post(path, serverFunctionHandler)],
  );

let shouldSSR = request =>
  switch (Dream.query(request, "ssr")) {
  | Some("false") => false
  | Some("true")
  | Some(_)
  | None => true
  };

/* esbuild names chunks and client components [name]-[hash], so a new build
   gives changed content a new name: those are kept for a year. Entry scripts
   and the stylesheet keep their names across builds, so they are revalidated
   with an ETag of their content on every use. */
let isHashed = path => {
  let name = Filename.remove_extension(Filename.basename(path));
  let length = String.length(name);
  length > 9
  && name.[length - 9] == '-'
  && String.for_all(
       character =>
         character >= 'A'
         && character <= 'Z'
         || character >= '0'
         && character <= '9',
       String.sub(name, length - 8, 8),
     );
};

let ifNoneMatch = (header, tag) =>
  String.trim(header) == "*"
  || List.exists(
       candidate => {
         let candidate = String.trim(candidate);
         candidate == tag || candidate == "W/" ++ tag;
       },
       String.split_on_char(',', header),
     );

let staticFile = (root, path, request) =>
  if (isHashed(path)) {
    let%lwt response = Dream.from_filesystem(root, path, request);
    if (Dream.status(response) == `OK) {
      Dream.set_header(
        response,
        "Cache-Control",
        "public, max-age=31536000, immutable",
      );
    };
    Lwt.return(response);
  } else {
    switch (Digest.file(Filename.concat(root, path))) {
    | exception _ => Dream.from_filesystem(root, path, request)
    | digest =>
      let headers = [
        ("Cache-Control", "public, no-cache"),
        ("ETag", "\"" ++ Digest.to_hex(digest) ++ "\""),
      ];
      switch (Dream.header(request, "If-None-Match")) {
      | Some(header) when ifNoneMatch(header, List.assoc("ETag", headers)) =>
        Dream.respond(~status=`Not_Modified, ~headers, "")
      | _ =>
        let%lwt response = Dream.from_filesystem(root, path, request);
        List.iter(
          ((name, value)) => Dream.set_header(response, name, value),
          headers,
        );
        Lwt.return(response);
      };
    };
  };

/* Pages read cookies and the APIs serve data that changes, so a shared cache
   never stores a response that does not say otherwise. */
let privateByDefault = (handler, request) => {
  let%lwt response = handler(request);
  if (Option.is_none(Dream.header(response, "Cache-Control"))) {
    Dream.set_header(response, "Cache-Control", "private, no-cache");
  };
  Lwt.return(response);
};

let server =
  Dream.logger @@
  privateByDefault @@
  Dream.router([
    getAndPost("/", Examples.Home.handler),
    Dream.get("/demo", req => Dream.redirect(req, "/")),
    Dream.get("/output.css", request =>
      staticFile("./_build/default/demo", "output.css", request)
    ),
    Dream.get(
      "/static/**",
      Dream.static(~loader=staticFile, "./_build/default/demo/client/dist"),
    ),
    getAndPost(Routes.renderToString, _request =>
      Dream.html(
        ReactDOM.renderToString(
          <Document script="/static/demo/RenderRoot.re.js">
            <App />
          </Document>,
        ),
      )
    ),
    getAndPost(Routes.renderToStaticMarkup, _request =>
      Dream.html(
        ReactDOM.renderToStaticMarkup(
          <Document script="/static/demo/HydrateRoot.re.js">
            <App />
          </Document>,
        ),
      )
    ),
    getAndPost(Routes.renderToStream, Examples.Comments.handler),
    getAndPost(Routes.singlePageRSC, Examples.SinglePageRSC.handler),
    getAndPost(Routes.serverOnlyRSC, Examples.ServerOnlyRSC.handler),
    Dream.get(Routes.ssrQuery, Examples.SsrQuery.handler),
    Dream.get(Routes.ssrQueryApi ++ "/:id", Examples.SsrQuery.api),
    Dream.post(Routes.ssrQueryApi ++ "/:id", Examples.SsrQuery.rename),
    Dream.get(SsrQueryDemo.pagesApi ++ "/:cursor", Examples.SsrQuery.page),
    ...List.concat([
         Demo_kitchen_sink_native.KsApi.routes,
         DreamRouter.routes(
           ~server=RouterRegistry.server,
           ~actionHandler=serverFunctionHandler,
           ~lookup=FunctionReferences.get,
           ~ssr=shouldSSR,
           ~bootstrapModules=["/static/demo/RouterDemo.re.js"],
           ~document=
             (~head, children) =>
               <Pages.Document head> children </Pages.Document>,
           (),
         ),
         DreamRouter.routes(
           ~server=Demo_hacker_news_router_native.RouterRegistry.server,
           ~actionHandler=serverFunctionHandler,
           ~lookup=FunctionReferences.get,
           ~ssr=shouldSSR,
           ~bootstrapModules=["/static/demo/RouterDemo.re.js"],
           ~pageCacheCapacity=32,
           ~document=
             (~head, children) =>
               <Demo_hacker_news_router_native.Pages.Document head>
                 children
               </Demo_hacker_news_router_native.Pages.Document>,
           (),
         ),
         DreamRouter.routes(
           ~server=Demo_kitchen_sink_native.RouterRegistry.server,
           ~actionHandler=serverFunctionHandler,
           ~lookup=FunctionReferences.get,
           ~ssr=shouldSSR,
           ~bootstrapModules=["/static/demo/RouterDemo.re.js"],
           ~document=
             (~head, children) =>
               <Demo_kitchen_sink_native.Pages.Document head>
                 children
               </Demo_kitchen_sink_native.Pages.Document>,
           (),
         ),
       ]),
  ]);

let interface = {
  switch (Sys.getenv_opt("SERVER_INTERFACE")) {
  | Some(env) => env
  | None => "localhost"
  };
};

let port = {
  switch (Sys.getenv_opt("PORT")) {
  | Some(env) =>
    switch (int_of_string_opt(env)) {
    | Some(port) => port
    | None => failwith("PORT must be a number, got: " ++ env)
    }
  | None => 8080
  };
};

IsoFetch.origin := "http://localhost:" ++ Int.to_string(port);

Dream.run(~port, ~interface, server);
