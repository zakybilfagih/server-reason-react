type theme =
  | Light
  | Dark
  | System;

type position =
  | Top
  | Bottom
  | Left
  | Right;

type buttonPosition =
  | TopLeft
  | TopRight
  | BottomLeft
  | BottomRight
  | Relative;

type errorType = {
  name: string,
  error: ReactQuery.CachedQuery.t => exn,
};

let themeToRaw =
  fun
  | Light => "light"
  | Dark => "dark"
  | System => "system";

let positionToRaw =
  fun
  | Top => "top"
  | Bottom => "bottom"
  | Left => "left"
  | Right => "right";

let buttonPositionToRaw =
  fun
  | TopLeft => "top-left"
  | TopRight => "top-right"
  | BottomLeft => "bottom-left"
  | BottomRight => "bottom-right"
  | Relative => "relative";

/* A Melange exception is the JS value TanStack stores as the error, and the
   query's error accessors give it back as it was raised. */
type rawErrorType;

[@mel.obj]
external rawErrorType:
  (~name: string, ~_initializer: (. ReactQuery.CachedQuery.t) => exn) =>
  rawErrorType;

let errorTypesToRaw =
  Array.map(({ name, error }: errorType) =>
    rawErrorType(~name, ~_initializer=(. query) => error(query))
  );

module Raw = {
  [@mel.module "@tanstack/react-query-devtools"] [@react.component]
  external make:
    (
      ~initialIsOpen: bool=?,
      ~buttonPosition: string=?,
      ~position: string=?,
      ~client: ReactQuery.QueryClient.t=?,
      ~errorTypes: array(rawErrorType)=?,
      ~hideDisabledQueries: bool=?,
      ~theme: string=?
    ) =>
    React.element =
    "ReactQueryDevtools";
};

module RawPanel = {
  [@mel.module "@tanstack/react-query-devtools"] [@react.component]
  external make:
    (
      ~client: ReactQuery.QueryClient.t=?,
      ~errorTypes: array(rawErrorType)=?,
      ~hideDisabledQueries: bool=?,
      ~theme: string=?,
      ~style: ReactDOM.Style.t=?,
      ~onClose: unit => unit=?
    ) =>
    React.element =
    "ReactQueryDevtoolsPanel";
};

/* The server renders nothing, so the devtools wait for hydration. */
let useMounted = () => {
  let (mounted, setMounted) = React.useState(() => false);
  React.useEffect0(() => {
    setMounted(_ => true);
    None;
  });
  mounted;
};

[@react.component]
let make =
    (
      ~initialIsOpen=?,
      ~buttonPosition=?,
      ~position=?,
      ~client=?,
      ~errorTypes=?,
      ~hideDisabledQueries=?,
      ~theme=?,
      (),
    ) =>
  useMounted()
    ? <Raw
        ?initialIsOpen
        buttonPosition=?{Option.map(buttonPositionToRaw, buttonPosition)}
        position=?{Option.map(positionToRaw, position)}
        ?client
        errorTypes=?{Option.map(errorTypesToRaw, errorTypes)}
        ?hideDisabledQueries
        theme=?{Option.map(themeToRaw, theme)}
      />
    : React.null;

module Panel = {
  [@react.component]
  let make =
      (
        ~client=?,
        ~errorTypes=?,
        ~hideDisabledQueries=?,
        ~theme=?,
        ~style=?,
        ~onClose=?,
        (),
      ) =>
    useMounted()
      ? <RawPanel
          ?client
          errorTypes=?{Option.map(errorTypesToRaw, errorTypes)}
          ?hideDisabledQueries
          theme=?{Option.map(themeToRaw, theme)}
          ?style
          ?onClose
        />
      : React.null;
};
