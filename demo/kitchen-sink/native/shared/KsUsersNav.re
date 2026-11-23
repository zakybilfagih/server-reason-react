/* UsersComponent */

let usersPath = () =>
  fst(KsLink.splitHref(RouterRuntime.href(Router.Users.destination())));

let compareBy = (sortBy: UsersView.sortBy, a: KsData.user, b: KsData.user) =>
  switch (sortBy) {
  | Name => a.name > b.name ? 1 : (-1)
  | Id => a.id > b.id ? 1 : (-1)
  | Email => a.email > b.email ? 1 : (-1)
  };

let isPending = (status: RouterRuntime.Navigation.status, ~userId) =>
  switch (status) {
  | Loading({ to_, _ }) =>
    to_.pathname == usersPath()
    ++ "/user"
    && KsMatches.value(KsMatches.parseSearch(to_.search), "userId")
    == Some(Int.to_string(userId))
  | Idle
  | Failed(_) => false
  };

[@react.client.component]
let make = (~children: React.element) => {
  let (navigate, status) = Router.useNavigation();
  let (values, updateSearch) = RouterReact.useSearch();
  let location = RouterReact.useLocation();
  let settings = KsSettings.use();
  let ({ Router.usersView }, _) = Router.useSearch();
  let view = Option.value(~default=[], usersView);
  let users =
    ReactQuery.useSuspenseQuery(
      KsData.usersQuery,
      KsData.usersQueryOfView(view),
    )
    |> ReactQuery.SuspenseResult.data;
  let sortBy = Option.value(~default=UsersView.Name, UsersView.sortBy(view));
  let filterBy = UsersView.filterBy(view);
  let (filterDraft, setFilterDraft) =
    React.useState(() => Option.value(~default="", filterBy));

  let%melange_only resetDraft = () => {
    setFilterDraft(_ => Option.value(~default="", filterBy));
    None;
  };
  React.useEffect1(resetDraft, [|filterBy|]);

  /* navigate({from: '/dashboard/users', search: old => ..., replace: true}):
     the users page, keeping every search param. */
  let%melange_only navigateUsers = update => {
    let next = [
      ("usersView", [UsersView.to_string(update(view))]),
      ...List.filter(((name, _)) => name != "usersView", values),
    ];
    let search = KsMatches.searchString(next);
    if (location.pathname == usersPath()) {
      settings.startTransition(
        {
          ...location,
          search,
          hash: "",
        },
        () => {
        updateSearch(
          ~owned=["usersView"],
          ~values=[List.hd(next)],
          ~history=Replace,
          (),
        )
        |> ignore
      });
    } else {
      navigate(
        ~history=Replace,
        RouterRuntime.destination(~path=usersPath() ++ search),
      )
      |> ignore;
    };
  };

  let%melange_only syncFilter = () => {
    navigateUsers(view => UsersView.setFilterBy(view, filterDraft));
    None;
  };
  React.useEffect1(syncFilter, [|filterDraft|]);

  let%melange_only onSort = event =>
    switch (UsersView.sortByOfString(React.Event.Form.target(event)##value)) {
    | Some(sortBy) =>
      navigateUsers(view => UsersView.setSortBy(view, sortBy))
    | None => ()
    };
  let%melange_only onFilter = event =>
    setFilterDraft(_ => React.Event.Form.target(event)##value);

  let sorted = List.sort(compareBy(sortBy), Array.to_list(users));
  let filtered =
    switch (filterBy) {
    | None
    | Some("") => sorted
    | Some(filterBy) =>
      let filterBy = String.lowercase_ascii(filterBy);
      List.filter(
        (user: KsData.user) =>
          Js.String.includes(
            ~search=filterBy,
            String.lowercase_ascii(user.name),
          ),
        sorted,
      );
    };

  <div className="flex-1 flex">
    <div className="divide-y">
      <div
        className="py-2 px-3 flex gap-2 items-center bg-gray-100 dark:bg-gray-800">
        <div> {React.string("Sort By:")} </div>
        <select
          value={UsersView.sortByToString(sortBy)}
          onChange=onSort
          className="flex-1 border p-1 px-2 rounded-sm">
          {[|"name", "id", "email"|]
           |> Array.map(d =>
                <option key=d value=d> {React.string(d)} </option>
              )
           |> React.array}
        </select>
      </div>
      <div
        className="py-2 px-3 flex gap-2 items-center bg-gray-100 dark:bg-gray-800">
        <div> {React.string("Filter By:")} </div>
        <input
          value=filterDraft
          onChange=onFilter
          placeholder="Search Names..."
          className="min-w-0 flex-1 border p-1 px-2 rounded-sm"
        />
      </div>
      {filtered
       |> List.map((user: KsData.user) =>
            <div key={Int.to_string(user.id)}>
              <KsLink
                destination={Router.User.destination(
                  ~search=
                    Router.User.search(~usersView?, ~userId=user.id, ()),
                  (),
                )}
                className="block py-2 px-3 text-blue-700">
                <pre className="text-sm">
                  {React.string(user.name ++ " ")}
                  <KsSpinner
                    show={isPending(status, ~userId=user.id)}
                    wait="delay-50"
                  />
                </pre>
              </KsLink>
            </div>
          )
       |> Array.of_list
       |> React.array}
    </div>
    <div className="flex-initial border-l"> children </div>
  </div>;
};
