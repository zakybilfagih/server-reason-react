/* The sandbox settings and the transition that search updates run in, so
   the spinner and pending outlets see a search update as a navigation. */

type t = {
  loaderDelay: int,
  setLoaderDelay: int => unit,
  pendingMs: int,
  setPendingMs: int => unit,
  pendingMinMs: int,
  setPendingMinMs: int => unit,
  /* The location a search update is moving to, while it suspends. */
  transitionTarget: option(RouterRuntime.Navigation.location),
  startTransition: (RouterRuntime.Navigation.location, unit => unit) => unit,
};

let context: React.Context.t(t) =
  React.createContext({
    loaderDelay: 500,
    setLoaderDelay: _ => (),
    pendingMs: 1000,
    setPendingMs: _ => (),
    pendingMinMs: 500,
    setPendingMinMs: _ => (),
    transitionTarget: None,
    startTransition: (_, update) => update(),
  });

module Provider = {
  [@react.component]
  let make = (~value: t, ~children: React.element) =>
    switch%platform () {
    | Native =>
      React.Context.provider(
        context,
        React.Context.makeProps(~value, ~children, ()),
      )
    | Melange =>
      React.createElement(
        React.Context.provider(context),
        {
          "value": value,
          "children": children,
        },
      )
    };
};

let use = () => React.useContext(context);
