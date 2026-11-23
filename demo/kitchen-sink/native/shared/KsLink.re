/* A TanStack Link: [activeProps={{className: "font-bold"}}], active when the
   path matches (exactly, or as a prefix at a segment boundary) and the
   link's search is part of the current one. */

let relative = pathname => {
  let path = String.concat("/", [""] @ KsMatches.segments(pathname));
  path == "" ? "/" : path;
};

let splitHref = href =>
  switch (String.index_opt(href, '?')) {
  | Some(index) => (
      String.sub(href, 0, index),
      String.sub(href, index, String.length(href) - index),
    )
  | None => (href, "")
  };

let includes = (~current, ~target) =>
  List.for_all(
    ((name, values)) =>
      switch (List.assoc_opt(name, current), values) {
      | (Some([current, ..._]), [target, ..._]) when name == "usersView" =>
        switch (UsersView.parse(current), UsersView.parse(target)) {
        | (Ok(current), Ok(target)) =>
          List.for_all(field => List.mem(field, current), target)
        | _ => current == target
        }
      | (Some(current), target) => current == target
      | (None, _) => false
      },
    target,
  );

let isActive =
    (~exact, ~location: RouterRuntime.Navigation.location, destination) => {
  let (pathname, search) = splitHref(RouterRuntime.href(destination));
  let current = relative(location.pathname);
  let next = relative(pathname);
  let pathMatches =
    exact
      ? current == next
      : next == "/"
          ? current == "/"
          : String.starts_with(~prefix=next, current)
            && (
              String.length(current) == String.length(next)
              || current.[String.length(next)] == '/'
            );
  pathMatches
  && includes(
       ~current=KsMatches.parseSearch(location.search),
       ~target=KsMatches.parseSearch(search),
     );
};

[@react.component]
let make = (~destination, ~exact=false, ~className, ~children) => {
  let location = RouterReact.useLocation();
  <RouterReact.Link
    destination
    className={
      isActive(~exact, ~location, destination)
        ? className ++ " font-bold" : className
    }>
    children
  </RouterReact.Link>;
};
