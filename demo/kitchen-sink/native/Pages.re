/* The example's routes. Like the example's router context, every loader of
   a request shares one QueryClient, and its ensureQueryData skips a query
   the browser already has data for. Otherwise it fetches the query and
   hands that query, dehydrated, to its components, which read it with
   useSuspenseQuery in the browser's client. */

let clientField: Dream.field(ReactQuery.QueryClient.t) =
  Dream.new_field(~name="query-client", ());

let requestClient = () =>
  switch (DreamRouter.request()) {
  | Some(request) =>
    switch (Dream.field(request, clientField)) {
    | Some(client) => client
    | None =>
      let client = KsData.makeQueryClient();
      Dream.set_field(request, clientField, client);
      client;
    }
  | None => KsData.makeQueryClient()
  };

let browserHas = hash =>
  switch (
    Option.bind(DreamRouter.request(), request =>
      Dream.header(request, KsData.queryCacheHeader)
    )
  ) {
  | Some(header) => List.mem(hash, KsData.queryCacheOfHeader(header))
  | None => false
  };

/* The client's dehydrated state with only the query at [hash]. */
let state = (client, hash) => {
  let queries =
    switch (ReactQuery.DehydratedState.toJson(ReactQuery.dehydrate(client))) {
    | `Assoc(fields) =>
      switch (List.assoc_opt("queries", fields)) {
      | Some(`List(queries)) =>
        List.filter(
          query =>
            Yojson.Basic.Util.member("queryHash", query) == `String(hash),
          queries,
        )
      | _ => []
      }
    | _ => []
    };
  Yojson.Basic.to_string(
    `Assoc([("queries", `List(queries)), ("mutations", `List([]))]),
  );
};

let ensureQueryData = (query, key) => {
  let hash = ReactQuery.Query.hash(query, key);
  if (browserHas(hash)) {
    Lwt.return(Router.Loader.Data(state(KsData.makeQueryClient(), hash)));
  } else {
    let client = requestClient();
    Lwt.catch(
      () =>
        ReactQuery.QueryClient.ensure(client, query, key)
        |> Lwt.map(_ => Router.Loader.Data(state(client, hash))),
      exn => Lwt.return(Router.Loader.Error(KsData.errorMessage(exn))),
    );
  };
};

let cookie = name =>
  Option.bind(DreamRouter.request(), request =>
    Dream.cookie(~decrypt=false, request, name)
  )
  |> Option.map(Dream.from_percent_encoded);

let intCookie = name => Option.bind(cookie(name), int_of_string_opt);

let styles = {|
html {
  color-scheme: light dark;
  background-color: oklch(98.5% 0.002 247.839);
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", "Noto Sans", Arial, sans-serif, "Apple Color Emoji", "Segoe UI Emoji", "Segoe UI Symbol", "Noto Color Emoji";
}
* { border-color: oklch(92.8% 0.006 264.531); }
@media (prefers-color-scheme: dark) {
  html { background-color: oklch(21% 0.034 264.665); }
  * { border-color: oklch(27.8% 0.033 256.848); }
}
|};

module Document = {
  [@react.component]
  let make = (~head, ~children) =>
    <html lang="en">
      <head>
        <meta charSet="UTF-8" />
        <meta
          name="viewport"
          content="width=device-width, initial-scale=1.0"
        />
        <link rel="stylesheet" href="/output.css" />
        <style dangerouslySetInnerHTML={ "__html": styles } />
        head
      </head>
      <body
        className="bg-gray-50 text-gray-950 dark:bg-gray-900 dark:text-gray-200">
        <KsProviders
          loaderDelay={intCookie("loaderDelay")}
          pendingMs={intCookie("pendingMs")}
          pendingMinMs={intCookie("pendingMinMs")}>
          children
        </KsProviders>
      </body>
    </html>;
};

module Root = {
  [@react.component]
  let make = (~search as _, ~children) =>
    <>
      <div className="min-h-screen flex flex-col">
        <div className="flex items-center border-b gap-2">
          <h1 className="text-3xl p-2"> {React.string("Kitchen Sink")} </h1>
          <div className="text-3xl"> <KsRouterSpinner /> </div>
        </div>
        <div className="flex-1 flex">
          <KsSidebar />
          <div className="flex-1 border-l">
            <KsPending depth=0> children </KsPending>
          </div>
        </div>
      </div>
      <KsDevtools />
    </>;
};

module NotFound = {
  [@react.component]
  let make = (~search as _, ~error as _) =>
    <p> {React.string("Not Found")} </p>;
};

module AppError = {
  let message: Router.Error.t(string) => string =
    fun
    | Application(message) => message
    | NotFound(_) => "Not Found"
    | InvalidPathParameter({ name })
    | InvalidSearchParameter({ name }) => "Invalid " ++ name
    | Internal(_) => "";

  [@react.component]
  let make = (~search as _, ~error) =>
    <KsErrorComponent message={message(error)} />;
};

module ScopeError = {
  [@react.component]
  let make = (~search as _, ~error) =>
    <KsErrorComponent message={AppError.message(error)} />;
};

module InvoiceError = {
  [@react.component]
  let make = (~invoiceId as _, ~search as _, ~invoices as _, ~error) =>
    <KsErrorComponent message={AppError.message(error)} />;
};

module UserError = {
  [@react.component]
  let make = (~search as _, ~users as _, ~error) =>
    <KsErrorComponent message={AppError.message(error)} />;
};

module Index = {
  [@react.component]
  let make = (~search as _) =>
    <div className="p-2">
      <div className="text-lg"> {React.string("Welcome Home!")} </div>
      <hr className="my-2" />
      <Router.Invoice
        invoiceId=3
        className="py-1 px-2 text-xs bg-blue-500 text-white rounded-full">
        {React.string("1 New Invoice")}
      </Router.Invoice>
      <hr className="my-2" />
      <div className="max-w-xl">
        {React.string(
           "As you navigate around take note of the UX. It should feel suspense-like, where routes are only rendered once all of their data and elements are ready.",
         )}
        <hr className="my-2" />
        {React.string(
           "To exaggerate async effects, play with the artificial request delay slider in the bottom-left corner.",
         )}
        <hr className="my-2" />
        {React.string(
           "The last 2 sliders determine if link-hover preloading is enabled (and how long those preloads stick around) and also whether to cache rendered route data (and for how long). Both of these default to 0 (or off).",
         )}
      </div>
    </div>;
};

module DashboardLayout = {
  [@react.component]
  let make = (~search as _, ~children) =>
    <>
      <div className="flex items-center border-b">
        <h2 className="text-xl p-2"> {React.string("Dashboard")} </h2>
      </div>
      <KsDashboardTabs />
      <hr />
      <KsPending depth=1> children </KsPending>
    </>;
};

module InvoicesLoader = {
  let load = (~search as _, ()) => ensureQueryData(KsData.invoicesQuery, ());
};

module DashboardIndex = {
  [@react.component]
  let make = (~search as _, ~invoices) =>
    <KsHydrate state=invoices> <KsDashboardSummary /> </KsHydrate>;
};

module InvoicesLayout = {
  [@react.component]
  let make = (~search as _, ~invoices, ~children) =>
    <KsHydrate state=invoices>
      <KsInvoicesNav>
        <KsPending depth=2> children </KsPending>
      </KsInvoicesNav>
    </KsHydrate>;
};

module InvoicesIndex = {
  [@react.component]
  let make = (~search as _, ~invoices as _) => <KsInvoiceCreate />;
};

module InvoiceLoader = {
  let load = (~invoiceId, ~search as _, ~invoices as _, ()) =>
    ensureQueryData(KsData.invoiceQuery, invoiceId);
};

module Invoice = {
  [@react.component]
  let make = (~invoiceId, ~search as _, ~invoices as _, ~invoice) =>
    <KsHydrate state=invoice>
      <KsInvoiceEdit key={Int.to_string(invoiceId)} invoiceId />
    </KsHydrate>;
};

module UsersLoader = {
  let load = (~search: Router.search, ()) =>
    ensureQueryData(
      KsData.usersQuery,
      KsData.usersQueryOfView(Option.value(~default=[], search.usersView)),
    );
};

module UsersLayout = {
  [@react.component]
  let make = (~search as _, ~users, ~children) =>
    <KsHydrate state=users>
      <KsUsersNav> <KsPending depth=2> children </KsPending> </KsUsersNav>
    </KsHydrate>;
};

module UsersIndex = {
  [@react.component]
  let make = (~search as _, ~users as _) =>
    <div className="p-2 space-y-2">
      <p>
        {React.string(
           "Normally, setting default search parameters would either need to be done manually in every link to a page, or as a side-effect (not a great experience).",
         )}
      </p>
      <p>
        {React.string("Instead, we can use ")}
        <strong> {React.string("search filters")} </strong>
        {React.string(
           " to provide defaults or even persist search params for links to routes (and child routes).",
         )}
      </p>
      <p>
        {React.string(
           "A good example of this is the sorting and filtering of the users list. In a traditional router, both would be lost while navigating around individual users or even changing each sort/filter option unless each state was manually passed from the current route into each new link we created (that's a lot of tedious and error-prone work). With TanStack router and search filters, they are persisted with little effort.",
         )}
      </p>
    </div>;
};

module UserLoader = {
  let load = (~search: Router.User.search, ~users as _, ()) =>
    ensureQueryData(KsData.userQuery, search.userId);
};

module User = {
  [@react.component]
  let make = (~search: Router.User.search, ~users as _, ~user) =>
    <KsHydrate state=user> <KsUserView userId={search.userId} /> </KsHydrate>;
};

module Expensive = {
  [@react.component]
  let make = (~search as _) =>
    <div className="p-2">
      {React.string(
         {js|I am an "expensive" component... which really just means that I was code-split 😉|js},
       )}
    </div>;
};

/* beforeLoad: logged out users go to the login page, which sends them back
   here once they log in. */
module AuthLoader = {
  let load = (~search as _, ()) =>
    Lwt.return(
      switch (cookie("auth"), DreamRouter.request()) {
      | (Some(username), _) => Router.Loader.Data(username)
      | (None, request) =>
        Router.Loader.Redirect(
          Router.Login.destination(
            ~search=
              Router.Login.search(
                ~redirect=?Option.map(Dream.target, request),
                (),
              ),
            (),
          ),
        )
      },
    );
};

module Profile = {
  [@react.component]
  let make = (~search as _, ~username) =>
    <div className="p-2 space-y-2">
      <div>
        {React.string("Username:")}
        <strong> {React.string(username)} </strong>
      </div>
    </div>;
};

module LoginLoader = {
  let load = (~search as _, ()) =>
    Lwt.return(Router.Loader.Data(cookie("auth")));
};

module Login = {
  [@react.component]
  let make = (~search: Router.Login.search, ~auth) =>
    <KsLogin username=auth redirect={search.redirect} />;
};

module PathlessLayout = {
  [@react.component]
  let make = (~search as _, ~children) =>
    <div>
      <div> {React.string("Pathless Layout")} </div>
      <hr />
      <KsPending depth=1> children </KsPending>
    </div>;
};

module RouteA = {
  [@react.component]
  let make = (~search as _) =>
    <div> <div> {React.string("I'm A")} </div> </div>;
};

module RouteB = {
  [@react.component]
  let make = (~search as _) =>
    <div> <div> {React.string({|i"m B|})} </div> </div>;
};
