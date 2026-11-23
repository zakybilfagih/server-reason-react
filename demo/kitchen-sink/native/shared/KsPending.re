/* An outlet that shows the pending component after [pendingMs] when the
   navigation's first new match is right below this layout, and keeps it for
   at least [pendingMinMs]. The old match stays mounted but hidden, so a
   search update's transition keeps waiting for its data. */

let target = (status: RouterRuntime.Navigation.status, settings: KsSettings.t) =>
  switch (status) {
  | Loading({ to_, _ }) => Some(to_)
  | Idle
  | Failed(_) => settings.transitionTarget
  };

[@react.client.component]
let make = (~depth: int, ~children: React.element) => {
  let (_, status) = Router.useNavigation();
  let location = RouterReact.useLocation();
  let settings = KsSettings.use();
  let active =
    switch (target(status, settings)) {
    | Some(target) =>
      KsMatches.firstDifference(
        0,
        KsMatches.ofLocation(location),
        KsMatches.ofLocation(target),
      )
      == Some(depth + 1)
    | None => false
    };
  let (shownAt, setShownAt) = React.useState(() => None);

  let%melange_only update = () =>
    switch (active, shownAt) {
    | (true, None) =>
      let timer =
        Js.Global.setTimeout(
          ~f=() => setShownAt(_ => Some(Js.Date.now())),
          settings.pendingMs,
        );
      Some(() => Js.Global.clearTimeout(timer));
    | (false, Some(at)) =>
      let remaining =
        float_of_int(settings.pendingMinMs) -. (Js.Date.now() -. at);
      let timer =
        Js.Global.setTimeout(
          ~f=() => setShownAt(_ => None),
          Int.max(0, int_of_float(remaining)),
        );
      Some(() => Js.Global.clearTimeout(timer));
    | (true, Some(_))
    | (false, None) => None
    };
  React.useEffect2(update, (active, shownAt));

  let showing = Option.is_some(shownAt);
  <>
    {showing ? <KsPendingComponent /> : React.null}
    <div
      style={ReactDOM.Style.make(~display=showing ? "none" : "contents", ())}>
      <React.Suspense fallback={<KsPendingComponent />}>
        children
      </React.Suspense>
    </div>
  </>;
};
