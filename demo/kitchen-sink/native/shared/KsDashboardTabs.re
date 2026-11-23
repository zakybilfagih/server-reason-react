[@react.client.component]
let make = () => {
  let (search, _) = Router.useSearch();
  <div className="flex flex-wrap divide-x">
    {[|
       (Router.Dashboard.destination(), "Summary", true),
       (Router.Invoices.destination(), "Invoices", false),
       (
         Router.Users.destination(
           ~search={ usersView: search.usersView },
           (),
         ),
         "Users",
         false,
       ),
     |]
     |> Array.map(((destination, label, exact)) =>
          <KsLink key=label destination exact className="p-2">
            {React.string(label)}
          </KsLink>
        )
     |> React.array}
  </div>;
};
