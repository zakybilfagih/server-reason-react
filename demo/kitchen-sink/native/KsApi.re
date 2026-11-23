/* The JSON API the queries and mutations call, from loaders and from the
   browser. A failure answers its message as the body. */

let json = value => Dream.json(Melange_json.to_string(value));

let fail = (~status=`Internal_Server_Error, exn) =>
  Dream.respond(~status, KsData.errorMessage(exn));

let id = request => int_of_string_opt(Dream.param(request, "id"));

let draft = request => {
  let%lwt body = Dream.body(request);
  Lwt.return(
    switch (KsData.draft_of_json(Melange_json.of_string(body))) {
    | draft => Some(draft)
    | exception _ => None
    },
  );
};

let respond = (encode, promise) =>
  Lwt.catch(() => Lwt.bind(promise, value => json(encode(value))), fail);

let routes = [
  Dream.get(KsData.api ++ "/invoices", _ =>
    respond(
      KsData.invoices_to_json,
      KsDb.fetchInvoices() |> Lwt.map(Array.of_list),
    )
  ),
  Dream.get(KsData.api ++ "/invoices/:id", request =>
    switch (id(request)) {
    | None => Dream.empty(`Not_Found)
    | Some(id) =>
      switch%lwt (KsDb.fetchInvoiceById(id)) {
      | Some(invoice) => json(KsData.invoice_to_json(invoice))
      | None => Dream.respond(~status=`Not_Found, "Invoice not found")
      | exception exn => fail(exn)
      }
    }
  ),
  Dream.post(KsData.api ++ "/invoices", request =>
    switch%lwt (draft(request)) {
    | None => Dream.empty(`Bad_Request)
    | Some(draft) =>
      respond(KsData.invoice_to_json, KsDb.postInvoice(draft))
    }
  ),
  Dream.post(KsData.api ++ "/invoices/:id", request =>
    switch%lwt (draft(request)) {
    | None => Dream.empty(`Bad_Request)
    | Some(draft) =>
      switch (id(request)) {
      | None => Dream.empty(`Not_Found)
      | Some(id) =>
        respond(
          KsData.invoice_to_json,
          KsDb.patchInvoice(~id, draft)
          |> Lwt.map(invoice =>
               Option.value(
                 ~default={
                            id,
                            title: draft.title,
                            body: draft.body,
                          }: KsData.invoice,
                 invoice,
               )
             ),
        )
      }
    }
  ),
  Dream.get(KsData.api ++ "/users", request =>
    respond(
      KsData.users_to_json,
      KsDb.fetchUsers(
        ~filterBy=Dream.query(request, "filterBy"),
        ~sortBy=
          Option.bind(
            Dream.query(request, "sortBy"),
            UsersView.sortByOfString,
          ),
      )
      |> Lwt.map(Array.of_list),
    )
  ),
  Dream.get(KsData.api ++ "/users/:id", request =>
    switch (id(request)) {
    | None => Dream.empty(`Not_Found)
    | Some(id) =>
      respond(
        Melange_json.Primitives.option_to_json(KsData.user_to_json),
        KsDb.fetchUserById(id),
      )
    }
  ),
];
