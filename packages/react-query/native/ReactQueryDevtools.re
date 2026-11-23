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

[@react.component]
let make =
    (
      ~initialIsOpen as _: option(bool)=?,
      ~buttonPosition as _: option(buttonPosition)=?,
      ~position as _: option(position)=?,
      ~client as _: option(ReactQuery.QueryClient.t)=?,
      ~errorTypes as _: option(array(errorType))=?,
      ~hideDisabledQueries as _: option(bool)=?,
      ~theme as _: option(theme)=?,
      (),
    ) => React.null;

module Panel = {
  [@react.component]
  let make =
      (
        ~client as _: option(ReactQuery.QueryClient.t)=?,
        ~errorTypes as _: option(array(errorType))=?,
        ~hideDisabledQueries as _: option(bool)=?,
        ~theme as _: option(theme)=?,
        ~style as _: option(ReactDOM.Style.t)=?,
        ~onClose as _: option(unit => unit)=?,
        (),
      ) => React.null;
};
