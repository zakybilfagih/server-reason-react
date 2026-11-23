/* LoginComponent. Auth is a session cookie, so the server's beforeLoad
   check sees it. */
let button = "text-sm bg-blue-500 text-white border inline-block py-1 px-2 rounded-sm";

[@react.client.component]
let make = (~username: option(string), ~redirect: option(string)) => {
  let (navigate, _) = Router.useNavigation();
  let location = RouterReact.useLocation();
  let (draft, setDraft) = React.useState(() => "");

  let%melange_only logout = _ => KsForm.setCookie("auth", None);
  let%melange_only onSubmit = event => {
    React.Event.Form.preventDefault(event);
    KsForm.setCookie("auth", Some(draft));
    navigate(
      ~history=Replace,
      ~revalidate=true,
      RouterRuntime.destination(~path=location.pathname ++ location.search),
    )
    |> ignore;
  };
  let%melange_only onChange = event =>
    setDraft(_ => React.Event.Form.target(event)##value);

  let%melange_only followRedirect = () => {
    switch (username, redirect) {
    | (Some(_), Some(redirect)) =>
      navigate(~history=Push, RouterRuntime.destination(~path=redirect))
      |> ignore
    | _ => ()
    };
    None;
  };
  React.useLayoutEffect2(followRedirect, (username, redirect));

  switch (username) {
  | Some(username) =>
    <div>
      {React.string("Logged in as ")}
      <strong> {React.string(username)} </strong>
      <div className="h-2" />
      <button onClick=logout className=button>
        {React.string("Log out")}
      </button>
      <div className="h-2" />
    </div>
  | None =>
    <div className="p-2">
      <div> {React.string("You must log in!")} </div>
      <div className="h-2" />
      <form onSubmit className="flex gap-2">
        <input
          value=draft
          onChange
          placeholder="Username"
          className="border p-1 px-2 rounded-sm"
        />
        <button onClick=logout className=button>
          {React.string("Login")}
        </button>
      </form>
    </div>
  };
};
