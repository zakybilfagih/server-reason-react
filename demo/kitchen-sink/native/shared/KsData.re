/* The example's mockTodos.ts and utils.tsx. The invoices and users live on
   the server (KsDb), so the same query functions run in loaders and in the
   browser. */
open Melange_json.Primitives;

[@deriving json]
type invoice = {
  id: int,
  title: string,
  body: string,
};

[@deriving json]
type draft = {
  title: string,
  body: string,
};

[@deriving json]
type geo = {
  lat: string,
  lng: string,
};

[@deriving json]
type address = {
  street: string,
  suite: string,
  city: string,
  zipcode: string,
  geo,
};

[@deriving json]
type company = {
  name: string,
  catchPhrase: string,
  bs: string,
};

[@deriving json]
type user = {
  id: int,
  name: string,
  username: string,
  email: string,
  address,
  phone: string,
  website: string,
  company,
};

[@deriving json]
type invoices = array(invoice);

[@deriving json]
type users = array(user);

let api = "/demo/kitchen-sink-api";

/* The hashes of the queries the browser has data for, as a percent-encoded
   JSON array: hashes hold any text a key does. */
let queryCacheHeader = "Query-Cache";

let queryCacheToHeader = hashes =>
  Js.Global.encodeURIComponent(
    Melange_json.to_string(list_to_json(string_to_json, hashes)),
  );

let queryCacheOfHeader = header =>
  switch (
    list_of_json(
      string_of_json,
      Melange_json.of_string(Js.Global.decodeURIComponent(header)),
    )
  ) {
  | hashes => hashes
  | exception _ => []
  };

/* The sandbox settings are session cookies, so the server reads them too. */
let cookie = name =>
  switch%platform () {
  | Native =>
    Option.bind(DreamRouter.request(), request =>
      Dream.cookie(~decrypt=false, request, name)
    )
  | Melange =>
    let cookies: string = [%mel.raw "document.cookie"];
    cookies
    |> String.split_on_char(';')
    |> List.find_map(pair =>
         switch (String.index_opt(pair, '=')) {
         | Some(index) when String.trim(String.sub(pair, 0, index)) == name =>
           Some(
             String.sub(pair, index + 1, String.length(pair) - index - 1),
           )
         | _ => None
         }
       );
  };

let wait = ms =>
  switch%platform () {
  | Native => Lwt_unix.sleep(float_of_int(ms) /. 1000.)
  | Melange =>
    let done_ = ();
    Js.Promise.make((~resolve, ~reject as _) =>
      ignore(Js.Global.setTimeout(~f=() => resolve(. done_), ms))
    );
  };

let delayFn = (name, fn) => {
  let delay =
    Option.bind(cookie(name), int_of_string_opt) |> Option.value(~default=0);
  wait(delay) |> Js.Promise.then_(fn);
};

let loaderDelayFn = fn => delayFn("loaderDelay", fn);
let actionDelayFn = fn => delayFn("actionDelay", fn);

let errorMessage =
  fun
  | Failure(message) => message
  | exn =>
    switch%platform () {
    | Native => Printexc.to_string(exn)
    | Melange =>
      switch (exn) {
      | Js.Exn.Error(error) =>
        Js.Exn.message(error) |> Option.value(~default="Error")
      | exn => Printexc.to_string(exn)
      }
    };

/* A failed response rejects with its body as the message. */
let decode = (decoder, { IsoFetch.ok, body, _ }) =>
  if (ok) {
    switch (decoder(Melange_json.of_string(body))) {
    | data => Js.Promise.resolve(data)
    | exception _ =>
      Js.Promise.reject(Failure("Unexpected response: " ++ body))
    };
  } else {
    Js.Promise.reject(Failure(body));
  };

let fetchInvoices = (~signal=?, ()) =>
  loaderDelayFn(() =>
    IsoFetch.request(~signal?, api ++ "/invoices")
    |> Js.Promise.then_(decode(invoices_of_json))
  );

let fetchInvoiceById = (~signal=?, id) =>
  loaderDelayFn(() =>
    IsoFetch.request(~signal?, api ++ "/invoices/" ++ Int.to_string(id))
    |> Js.Promise.then_(decode(invoice_of_json))
  );

let postInvoice = (draft: draft) =>
  actionDelayFn(() =>
    IsoFetch.request(
      ~method_=`POST,
      ~body=Melange_json.to_string(draft_to_json(draft)),
      api ++ "/invoices",
    )
    |> Js.Promise.then_(decode(invoice_of_json))
  );

let patchInvoice = (invoice: invoice) =>
  actionDelayFn(() =>
    IsoFetch.request(
      ~method_=`POST,
      ~body=
        Melange_json.to_string(
          draft_to_json({
            title: invoice.title,
            body: invoice.body,
          }),
        ),
      api ++ "/invoices/" ++ Int.to_string(invoice.id),
    )
    |> Js.Promise.then_(decode(invoice_of_json))
  );

type usersQuery = {
  filterBy: option(string),
  sortBy: option(UsersView.sortBy),
};

let usersQueryOfView = view => {
  filterBy: UsersView.filterBy(view),
  sortBy: UsersView.sortBy(view),
};

let fetchUsers = (~signal=?, { filterBy, sortBy }) => {
  let query =
    [
      Option.map(
        filterBy => "filterBy=" ++ Js.Global.encodeURIComponent(filterBy),
        filterBy,
      ),
      Option.map(
        sortBy => "sortBy=" ++ UsersView.sortByToString(sortBy),
        sortBy,
      ),
    ]
    |> List.filter_map(Fun.id)
    |> String.concat("&");
  loaderDelayFn(() =>
    IsoFetch.request(
      ~signal?,
      api ++ "/users" ++ (query == "" ? "" : "?" ++ query),
    )
    |> Js.Promise.then_(decode(users_of_json))
  );
};

let fetchUserById = (~signal=?, id) =>
  loaderDelayFn(() =>
    IsoFetch.request(~signal?, api ++ "/users/" ++ Int.to_string(id))
    |> Js.Promise.then_(decode(option_of_json(user_of_json)))
  );

/* With SSR, a default staleTime above 0 keeps the browser from refetching
   what the server just fetched. */
let makeQueryClient = () =>
  ReactQuery.QueryClient.make(~staleTime=Ms(60. *. 1000.), ());

/* Query names must be unique, so the single invoice and user queries are
   [invoice] and [user] where TanStack's keys start with [invoices] and
   [users]. */
let invoicesQuery =
  ReactQuery.Query.make(
    ~name="invoices",
    ~key=() => [],
    ~codec=
      ReactQuery.Codec.make(
        ~encode=invoices_to_json,
        ~decode=invoices_of_json,
      ),
    ~fetch=(~signal, ()) => fetchInvoices(~signal, ()),
    (),
  );

let invoiceQuery =
  ReactQuery.Query.make(
    ~name="invoice",
    ~key=id => [ReactQuery.Key.int(id)],
    ~codec=
      ReactQuery.Codec.make(~encode=invoice_to_json, ~decode=invoice_of_json),
    ~fetch=(~signal, id) => fetchInvoiceById(~signal, id),
    (),
  );

let usersQuery =
  ReactQuery.Query.make(
    ~name="users",
    ~key=
      ({ filterBy, sortBy }) =>
        [
          ReactQuery.Key.obj(
            List.filter_map(
              Fun.id,
              [
                Option.map(
                  filterBy => ("filterBy", ReactQuery.Key.string(filterBy)),
                  filterBy,
                ),
                Option.map(
                  sortBy =>
                    (
                      "sortBy",
                      ReactQuery.Key.string(
                        UsersView.sortByToString(sortBy),
                      ),
                    ),
                  sortBy,
                ),
              ],
            ),
          ),
        ],
    ~codec=
      ReactQuery.Codec.make(~encode=users_to_json, ~decode=users_of_json),
    ~fetch=(~signal, query) => fetchUsers(~signal, query),
    (),
  );

let userQuery =
  ReactQuery.Query.make(
    ~name="user",
    ~key=id => [ReactQuery.Key.int(id)],
    ~codec=ReactQuery.Codec.make(~encode=user_to_json, ~decode=user_of_json),
    ~fetch=
      (~signal, id) =>
        fetchUserById(~signal, id)
        |> Js.Promise.then_(
             fun
             | Some(user) => Js.Promise.resolve(user)
             | None => Js.Promise.reject(Failure("User not found.")),
           ),
    (),
  );

let createInvoiceMutation =
  ReactQuery.Mutation.make(
    ~name="invoices-create",
    ~fn=postInvoice,
    ~onSuccess=
      (client, _, _, _) =>
        ReactQuery.QueryClient.invalidate(client, ReactQuery.Filter.all),
    (),
  );

let updateInvoiceMutation =
  ReactQuery.Mutation.make(
    ~name="invoices-update",
    ~fn=patchInvoice,
    ~onSuccess=
      (client, _, _, _) =>
        ReactQuery.QueryClient.invalidate(client, ReactQuery.Filter.all),
    (),
  );

/* JSON.stringify(user, null, 2), in jsonplaceholder's field order. */
let stringifyUser = (user: user) => {
  let string = UsersView.jsonString;
  let object_ = (indent, fields) => {
    let inner = indent ++ "  ";
    "{\n"
    ++ String.concat(
         ",\n",
         List.map(
           ((key, value)) => inner ++ string(key) ++ ": " ++ value,
           fields,
         ),
       )
    ++ "\n"
    ++ indent
    ++ "}";
  };
  object_(
    "",
    [
      ("id", Int.to_string(user.id)),
      ("name", string(user.name)),
      ("username", string(user.username)),
      ("email", string(user.email)),
      (
        "address",
        object_(
          "  ",
          [
            ("street", string(user.address.street)),
            ("suite", string(user.address.suite)),
            ("city", string(user.address.city)),
            ("zipcode", string(user.address.zipcode)),
            (
              "geo",
              object_(
                "    ",
                [
                  ("lat", string(user.address.geo.lat)),
                  ("lng", string(user.address.geo.lng)),
                ],
              ),
            ),
          ],
        ),
      ),
      ("phone", string(user.phone)),
      ("website", string(user.website)),
      (
        "company",
        object_(
          "  ",
          [
            ("name", string(user.company.name)),
            ("catchPhrase", string(user.company.catchPhrase)),
            ("bs", string(user.company.bs)),
          ],
        ),
      ),
    ],
  );
};
