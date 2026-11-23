/* The ids of the TanStack route matches a location renders, outermost
   first. A match is new when its params or loader deps change, and the
   router shows the pending component in the outlet above the first new
   match. */

let basePath = "/demo/kitchen-sink";

let segments = pathname => {
  let pathname =
    String.starts_with(~prefix=basePath, pathname)
      ? String.sub(
          pathname,
          String.length(basePath),
          String.length(pathname) - String.length(basePath),
        )
      : pathname;
  String.split_on_char('/', pathname) |> List.filter(segment => segment != "");
};

let value = (search, name) =>
  switch (List.assoc_opt(name, search)) {
  | Some([value, ..._]) => Some(value)
  | _ => None
  };

let usersDeps = search =>
  switch (Option.map(UsersView.parse, value(search, "usersView"))) {
  | Some(Ok(view)) =>
    "users:"
    ++ Option.fold(
         ~none="",
         ~some=UsersView.sortByToString,
         UsersView.sortBy(view),
       )
    ++ "|"
    ++ Option.value(~default="", UsersView.filterBy(view))
  | Some(Error(_))
  | None => "users:|"
  };

let chain = (~pathname, ~search) => [
  "root",
  ...switch (segments(pathname)) {
     | [] => ["index"]
     | ["dashboard"] => ["dashboard", "dashboard-index"]
     | ["dashboard", "invoices"] => [
         "dashboard",
         "invoices",
         "invoices-index",
       ]
     | ["dashboard", "invoices", id] => [
         "dashboard",
         "invoices",
         "invoice:" ++ id,
       ]
     | ["dashboard", "users"] => [
         "dashboard",
         usersDeps(search),
         "users-index",
       ]
     | ["dashboard", "users", "user"] => [
         "dashboard",
         usersDeps(search),
         "user:" ++ Option.value(~default="", value(search, "userId")),
       ]
     | ["expensive"] => ["expensive"]
     | ["profile"] => ["auth", "profile"]
     | ["login"] => ["login"]
     | ["route-a"] => ["pathless", "route-a"]
     | ["route-b"] => ["pathless", "route-b"]
     | _ => ["not-found"]
     },
];

let rec firstDifference = (index, current, target) =>
  switch (current, target) {
  | ([a, ...current], [b, ...target]) when a == b =>
    firstDifference(index + 1, current, target)
  | ([], []) => None
  | _ => Some(index)
  };

let decode = value =>
  switch (
    Js.Global.decodeURIComponent(
      String.concat(" ", String.split_on_char('+', value)),
    )
  ) {
  | decoded => decoded
  | exception _ => value
  };

let parseSearch = search =>
  (
    String.starts_with(~prefix="?", search)
      ? String.sub(search, 1, String.length(search) - 1) : search
  )
  |> String.split_on_char('&')
  |> List.filter(pair => pair != "")
  |> List.map(pair =>
       switch (String.index_opt(pair, '=')) {
       | Some(index) => (
           decode(String.sub(pair, 0, index)),
           [
             decode(
               String.sub(pair, index + 1, String.length(pair) - index - 1),
             ),
           ],
         )
       | None => (decode(pair), [""])
       }
     );

let ofLocation = (location: RouterRuntime.Navigation.location) =>
  chain(~pathname=location.pathname, ~search=parseSearch(location.search));

let searchString = values =>
  switch (
    values
    |> List.sort(((a, _), (b, _)) => String.compare(a, b))
    |> List.concat_map(((name, values)) =>
         List.map(
           value =>
             Js.Global.encodeURIComponent(name)
             ++ "="
             ++ Js.Global.encodeURIComponent(value),
           values,
         )
       )
  ) {
  | [] => ""
  | pairs => "?" ++ String.concat("&", pairs)
  };
