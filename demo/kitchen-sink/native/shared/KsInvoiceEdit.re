/* InvoiceComponent */
[@react.client.component]
let make = (~invoiceId: int) => {
  let (values, updateSearch) = RouterReact.useSearch();
  let showNotes = KsMatches.value(values, "showNotes") == Some("true");
  let invoice =
    ReactQuery.useSuspenseQuery(KsData.invoiceQuery, invoiceId)
    |> ReactQuery.SuspenseResult.data;
  let mutation = ReactQuery.useMutation(KsData.updateInvoiceMutation);
  let isPending = ReactQuery.MutationResult.isPending(mutation);
  let (submittedAt, setSubmittedAt) = React.useState(() => 0);
  let (notes, setNotes) =
    React.useState(() =>
      Option.value(~default="", KsMatches.value(values, "notes"))
    );

  let%melange_only syncNotes = () => {
    updateSearch(
      ~owned=["notes"],
      ~values=notes == "" ? [] : [("notes", [notes])],
      ~history=Replace,
      (),
    )
    |> ignore;
    None;
  };
  React.useEffect1(syncNotes, [|notes|]);

  let%melange_only onSubmit = event => {
    React.Event.Form.preventDefault(event);
    React.Event.Form.stopPropagation(event);
    setSubmittedAt(count => count + 1);
    ReactQuery.MutationResult.mutate(
      mutation,
      {
        id: invoice.id,
        title: KsForm.get(event, "title"),
        body: KsForm.get(event, "body"),
      }: KsData.invoice,
    );
  };
  let%melange_only onChangeNotes = event =>
    setNotes(_ => React.Event.Form.target(event)##value);

  let notesValues =
    List.filter(((name, _)) => name != "showNotes", values)
    @ (showNotes ? [] : [("showNotes", ["true"])]);
  let toggleNotes =
    RouterRuntime.destination(
      ~path=
        fst(
          KsLink.splitHref(
            RouterRuntime.href(Router.Invoice.destination(~invoiceId, ())),
          ),
        )
        ++ KsMatches.searchString(notesValues),
    );

  <form key={Int.to_string(invoice.id)} onSubmit className="p-2 space-y-2">
    <KsInvoiceFields invoice={Some(invoice)} disabled=isPending />
    <div>
      <RouterReact.Link destination=toggleNotes className="text-blue-700">
        {React.string((showNotes ? "Close Notes" : "Show Notes") ++ " ")}
      </RouterReact.Link>
      {showNotes
         ? <div>
             <div className="h-2" />
             <textarea
               value=notes
               onChange=onChangeNotes
               rows=5
               className="shadow-sm w-full p-2 rounded-sm"
               placeholder="Write some notes here..."
             />
             <div className="italic text-xs">
               {React.string(
                  "Notes are stored in the URL. Try copying the URL into a new tab!",
                )}
             </div>
           </div>
         : React.null}
    </div>
    <div>
      <button className=KsForm.submitClass disabled=isPending>
        {React.string("Save")}
      </button>
    </div>
    {switch (ReactQuery.MutationResult.status(mutation)) {
     | Succeeded(_, variables) when variables.id == invoice.id =>
       <div key={Int.to_string(submittedAt)}>
         <div className={KsForm.bounce("bg-green-500")}>
           {React.string("Saved!")}
         </div>
       </div>
     | Failed(_, variables) when variables.id == invoice.id =>
       <div key={Int.to_string(submittedAt)}>
         <div className={KsForm.bounce("bg-red-500")}>
           {React.string("Failed to save.")}
         </div>
       </div>
     | Running(variables) when variables.id == invoice.id =>
       <div key={Int.to_string(submittedAt)} />
     | _ => React.null
     }}
  </form>;
};
