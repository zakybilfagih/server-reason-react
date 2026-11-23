Router.make ~basePath:"/demo/kitchen-sink" ~layout:Pages.Root ~notFound:Pages.NotFound ~error:Pages.AppError
  ~search:{ usersView = Router.Search.optional UsersView.t }
  [
    Router.route Home ~page:Pages.Index ~path:"/";
    Router.group ~path:"/dashboard" ~layout:Pages.DashboardLayout
      [
        Router.route Dashboard ~page:Pages.DashboardIndex ~path:"/" ~loader:Pages.InvoicesLoader ~loaderLabel:invoices
          ~error:Pages.ScopeError;
        Router.group ~path:"/invoices" ~layout:Pages.InvoicesLayout ~loader:Pages.InvoicesLoader ~loaderLabel:invoices
          ~error:Pages.ScopeError
          [
            Router.route Invoices ~page:Pages.InvoicesIndex ~path:"/";
            Router.route Invoice ~page:Pages.Invoice ~path:"/:invoiceId<int>"
              ~search:{ showNotes = Router.Search.optional bool; notes = Router.Search.optional string }
              ~loader:Pages.InvoiceLoader ~loaderLabel:invoice ~error:Pages.InvoiceError;
          ];
        Router.group ~path:"/users" ~layout:Pages.UsersLayout ~loader:Pages.UsersLoader ~loaderLabel:users
          ~error:Pages.ScopeError
          [
            Router.route Users ~page:Pages.UsersIndex ~path:"/";
            Router.route User ~page:Pages.User ~path:"/user"
              ~search:{ userId = Router.Search.required int }
              ~loader:Pages.UserLoader ~loaderLabel:user ~error:Pages.UserError;
          ];
      ];
    Router.route Expensive ~page:Pages.Expensive ~path:"/expensive";
    Router.group ~path:"/" ~loader:Pages.AuthLoader ~loaderLabel:username
      [ Router.route Profile ~page:Pages.Profile ~path:"/profile" ];
    Router.route Login ~page:Pages.Login ~path:"/login"
      ~search:{ redirect = Router.Search.optional string }
      ~loader:Pages.LoginLoader ~loaderLabel:auth;
    Router.group ~path:"/" ~layout:Pages.PathlessLayout
      [
        Router.route RouteA ~page:Pages.RouteA ~path:"/route-a"; Router.route RouteB ~page:Pages.RouteB ~path:"/route-b";
      ];
  ]
