/* The example's mockTodos.ts on the server: the first ten posts and users
   of jsonplaceholder, fetched once and then changed in memory. */

let fetchJson = path => {
  let%lwt (response, body) =
    Cohttp_lwt_unix.Client.get(
      Uri.of_string("https://jsonplaceholder.typicode.com/" ++ path),
    );
  let%lwt body = Cohttp_lwt.Body.to_string(body);
  switch (Cohttp.Response.status(response)) {
  | `OK => Lwt.return(Yojson.Basic.from_string(body))
  | status =>
    Lwt.fail(
      Failure(
        "jsonplaceholder answered " ++ Cohttp.Code.string_of_status(status),
      ),
    )
  };
};

let firstTen = json =>
  Yojson.Basic.Util.to_list(json) |> List.filteri((index, _) => index < 10);

/* One fetch, shared by every caller; a failed one is tried again. */
let once = load => {
  let pending = ref(None);
  () =>
    switch (pending^) {
    | Some(promise) => promise
    | None =>
      let promise =
        Lwt.catch(
          load,
          exn => {
            pending := None;
            Lwt.fail(exn);
          },
        );
      pending := Some(promise);
      promise;
    };
};

let invoices: ref(list(KsData.invoice)) = ref([]);
let users: ref(list(KsData.user)) = ref([]);

let ensureInvoices =
  once(() => {
    let%lwt json = fetchJson("posts");
    invoices :=
      List.map(
        (post): KsData.invoice =>
          {
            id: Yojson.Basic.Util.(member("id", post) |> to_int),
            title: Yojson.Basic.Util.(member("title", post) |> to_string),
            body: Yojson.Basic.Util.(member("body", post) |> to_string),
          },
        firstTen(json),
      );
    Lwt.return();
  });

let ensureUsers =
  once(() => {
    let%lwt json = fetchJson("users");
    users := List.map(KsData.user_of_json, firstTen(json));
    Lwt.return();
  });

let contains = (~search, value) => {
  let length = String.length(search);
  let rec from = index =>
    index
    + length <= String.length(value)
    && (String.sub(value, index, length) == search || from(index + 1));
  from(0);
};

let fetchInvoices = () => {
  let%lwt () = ensureInvoices();
  Lwt.return(invoices^);
};

let fetchInvoiceById = id => {
  let%lwt () = ensureInvoices();
  Lwt.return(
    List.find_opt((invoice: KsData.invoice) => invoice.id == id, invoices^),
  );
};

let postInvoice = (draft: KsData.draft) => {
  let%lwt () = ensureInvoices();
  if (contains(~search="error", draft.title)) {
    Lwt.fail(Failure("Ouch!"));
  } else {
    let invoice: KsData.invoice = {
      id: List.length(invoices^) + 1,
      title: draft.title,
      body: draft.body,
    };
    invoices := invoices^ @ [invoice];
    Lwt.return(invoice);
  };
};

let patchInvoice = (~id, draft: KsData.draft) => {
  let%lwt () = ensureInvoices();
  if (!List.exists((invoice: KsData.invoice) => invoice.id == id, invoices^)) {
    Lwt.fail(Failure("Invoice not found."));
  } else if (contains(~search="error", String.lowercase_ascii(draft.title))) {
    Lwt.fail(Failure("Ouch!"));
  } else {
    invoices :=
      List.map(
        (invoice: KsData.invoice) =>
          invoice.id == id
            ? {
              ...invoice,
              title: draft.title,
              body: draft.body,
            }
            : invoice,
        invoices^,
      );
    fetchInvoiceById(id);
  };
};

let fetchUsers = (~filterBy, ~sortBy) => {
  let%lwt () = ensureUsers();
  let users =
    switch (filterBy) {
    | Some(filterBy) when filterBy != "" =>
      List.filter(
        (user: KsData.user) =>
          contains(
            ~search=String.lowercase_ascii(filterBy),
            String.lowercase_ascii(user.name),
          ),
        users^,
      )
    | _ => users^
    };
  Lwt.return(
    switch (sortBy) {
    | Some(sortBy) => List.sort(KsUsersNav.compareBy(sortBy), users)
    | None => users
    },
  );
};

let fetchUserById = id => {
  let%lwt () = ensureUsers();
  Lwt.return(List.find_opt((user: KsData.user) => user.id == id, users^));
};
