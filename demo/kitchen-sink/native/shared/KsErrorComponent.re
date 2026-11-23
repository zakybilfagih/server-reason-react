/* TanStack Router's ErrorComponent. */
[@react.client.component]
let make = (~message: string) => {
  let (show, setShow) = React.useState(() => true);
  <div style={ReactDOM.Style.make(~padding=".5rem", ~maxWidth="100%", ())}>
    <div
      style={ReactDOM.Style.unsafeAddProp(
        ReactDOM.Style.make(~display="flex", ~alignItems="center", ()),
        "gap",
        ".5rem",
      )}>
      <strong style={ReactDOM.Style.make(~fontSize="1rem", ())}>
        {React.string("Something went wrong!")}
      </strong>
      <button
        style={
          ReactDOM.Style.make(
            ~fontSize=".6em",
            ~border="1px solid currentColor",
            ~padding=".1rem .2rem",
            ~fontWeight="bold",
            ~borderRadius=".25rem",
            (),
          )
          ->ReactDOM.Style.unsafeAddProp("appearance", "none")
        }
        onClick=[%melange_only _ => setShow(show => !show)]>
        {React.string(show ? "Hide Error" : "Show Error")}
      </button>
    </div>
    <div style={ReactDOM.Style.make(~height=".25rem", ())} />
    {show
       ? <div>
           <pre
             style={ReactDOM.Style.make(
               ~fontSize=".7em",
               ~border="1px solid red",
               ~borderRadius=".25rem",
               ~padding=".3rem",
               ~color="red",
               ~overflow="auto",
               (),
             )}>
             {message == ""
                ? React.null : <code> {React.string(message)} </code>}
           </pre>
         </div>
       : React.null}
  </div>;
};
