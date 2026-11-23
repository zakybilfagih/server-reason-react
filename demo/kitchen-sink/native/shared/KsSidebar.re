[@react.client.component]
let make = () =>
  <div className="divide-y w-56">
    {[|
       (Router.Home.destination(), "Home"),
       (Router.Dashboard.destination(), "Dashboard"),
       (Router.Expensive.destination(), "Expensive"),
       (Router.RouteA.destination(), "Pathless Layout A"),
       (Router.RouteB.destination(), "Pathless Layout B"),
       (Router.Profile.destination(), "Profile"),
       (Router.Login.destination(), "Login"),
     |]
     |> Array.map(((destination, label)) =>
          <div key={RouterRuntime.href(destination)}>
            <KsLink destination className="block py-2 px-3 text-blue-700">
              {React.string(label)}
            </KsLink>
          </div>
        )
     |> React.array}
  </div>;
