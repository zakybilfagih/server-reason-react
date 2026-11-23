let handler = _request =>
  Dream.stream(
    ~headers=[("Content-Type", "text/html")],
    response_stream => {
      let loader = ReactQuery.QueryClient.make();
      let%lwt () =
        ReactQuery.QueryClient.prefetch(
          loader,
          SsrQueryDemo.noteQuery,
          SsrQueryDemo.peekId,
        );
      let loaderState = ReactQuery.dehydrate(loader);
      let data =
        ReactQuery.DehydratedState.toJson(loaderState)
        |> Yojson.Basic.to_string
        |> String.split_on_char('<')
        |> String.concat("\\u003c");
      let client = ReactQuery.QueryClient.make();
      SsrQueryDemo.prefetch(client);
      let%lwt (stream, _abort) =
        ReactQuery.Server.renderToStream(
          ~env=`Dev,
          ~client,
          <Document script="/static/demo/SsrQuery.re.js" async=true data>
            <ReactQuery.PersistQueryClientProvider
              client persister=SsrQueryDemo.persister>
              <SsrQueryDemo loaderState />
            </ReactQuery.PersistQueryClientProvider>
          </Document>,
        );
      Lwt_stream.iter_s(
        chunk => {
          let%lwt () = Dream.write(response_stream, chunk);
          Dream.flush(response_stream);
        },
        stream,
      );
    },
  );

let findNote = id =>
  Lwt.map(
    fun
    | Ok(notes) => List.find_opt((note: Note.t) => note.id == id, notes)
    | Error(_) => None,
    DB.readNotes(),
  );

let rename = request =>
  switch (int_of_string_opt(Dream.param(request, "id"))) {
  | None => Dream.empty(`Not_Found)
  | Some(id) =>
    let%lwt body = Dream.body(request);
    let title =
      switch (SsrQueryDemo.rename_of_json(Melange_json.of_string(body))) {
      | { title } => Some(String.trim(title))
      | exception _ => None
      };
    let%lwt note = findNote(id);
    let%lwt () = Lwt_unix.sleep(0.5);
    switch (title, note) {
    | (None | Some(""), _) =>
      Dream.respond(~status=`Bad_Request, "A title is required")
    | (_, None) =>
      Dream.respond(~status=`Not_Found, SsrQueryDemo.notFound(id))
    | (Some(title), _)
        when String.length(title) > SsrQueryDemo.maxTitleLength =>
      Dream.respond(
        ~status=`Status(422),
        Printf.sprintf(
          "Titles are at most %d characters, this one has %d",
          SsrQueryDemo.maxTitleLength,
          String.length(title),
        ),
      )
    | (Some(title), Some(note)) =>
      switch%lwt (DB.editNote(~id, ~title, ~content=note.content)) {
      | Ok(note) =>
        Dream.json(
          Melange_json.to_string(
            SsrQueryDemo.loaded_to_json({
              loadedBy: "the rename",
              note,
            }),
          ),
        )
      | Error(_) =>
        Dream.respond(
          ~status=`Internal_Server_Error,
          "The note could not be saved",
        )
      }
    };
  };

/* Slower for higher ids, so the notes stream in one at a time. */
let api = request =>
  switch (int_of_string_opt(Dream.param(request, "id"))) {
  | None => Dream.empty(`Not_Found)
  | Some(id) =>
    let%lwt () = Lwt_unix.sleep(0.6 *. Float.of_int(id));
    let loadedBy =
      Dream.header(request, "User-Agent") == Some(IsoFetch.userAgent)
        ? "the server render" : "a client fetch";
    switch%lwt (findNote(id)) {
    | Some(note) =>
      Dream.json(
        Melange_json.to_string(
          SsrQueryDemo.loaded_to_json({
            loadedBy,
            note,
          }),
        ),
      )
    | None => Dream.respond(~status=`Not_Found, SsrQueryDemo.notFound(id))
    };
  };

/* One note per page, after a delay so loading a page shows. */
let page = request =>
  switch (int_of_string_opt(Dream.param(request, "cursor"))) {
  | None => Dream.empty(`Not_Found)
  | Some(cursor) =>
    let%lwt () = Lwt_unix.sleep(0.4);
    let%lwt notes =
      Lwt.map(
        fun
        | Ok(notes) => notes
        | Error(_) => [],
        DB.readNotes(),
      );
    let notes =
      List.sort((a: Note.t, b: Note.t) => Int.compare(a.id, b.id), notes);
    let total = List.length(notes);
    Dream.json(
      Melange_json.to_string(
        SsrQueryDemo.notePage_to_json({
          notes:
            notes
            |> List.filteri((index, _) => index == cursor)
            |> Array.of_list,
          nextCursor: cursor + 1 < total ? Some(cursor + 1) : None,
        }),
      ),
    );
  };
