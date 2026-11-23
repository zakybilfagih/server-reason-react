/** TanStack Query's devtools. They mount after hydration, so the server
    renders nothing. Like TanStack's, they render nothing in production
    builds. */

type theme =
  | Light
  | Dark
  | System;

/** The side of the screen the panel opens on. */
type position =
  | Top
  | Bottom
  | Left
  | Right;

/** Where the toggle button goes: a corner, or [Relative] to the page. */
type buttonPosition =
  | TopLeft
  | TopRight
  | BottomLeft
  | BottomRight
  | Relative;

/** A custom error the devtools can put on the selected query, to try its
    error state. The query's error is the exception [error] returns. */
type errorType = {
  name: string,
  error: ReactQuery.CachedQuery.t => exn,
};

[@react.component]
let make:
  (
    ~initialIsOpen: bool=?,
    ~buttonPosition: buttonPosition=?,
    ~position: position=?,
    ~client: ReactQuery.QueryClient.t=?,
    ~errorTypes: array(errorType)=?,
    ~hideDisabledQueries: bool=?,
    ~theme: theme=?,
    unit
  ) =>
  React.element;

/** The devtools inline in the page, always open, without the toggle
    button. [onClose] runs when the panel's close button is clicked. */
module Panel: {
  [@react.component]
  let make:
    (
      ~client: ReactQuery.QueryClient.t=?,
      ~errorTypes: array(errorType)=?,
      ~hideDisabledQueries: bool=?,
      ~theme: theme=?,
      ~style: ReactDOM.Style.t=?,
      ~onClose: unit => unit=?,
      unit
    ) =>
    React.element;
};
