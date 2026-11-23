/* InvoicesIndexComponent */
[@react.client.component]
let make = () => {
  let mutation = ReactQuery.useMutation(KsData.createInvoiceMutation);
  let status = ReactQuery.MutationResult.status(mutation);
  let isPending = ReactQuery.MutationResult.isPending(mutation);
  let%melange_only onSubmit = event => {
    React.Event.Form.preventDefault(event);
    React.Event.Form.stopPropagation(event);
    ReactQuery.MutationResult.mutate(
      mutation,
      {
        title: KsForm.get(event, "title"),
        body: KsForm.get(event, "body"),
      }: KsData.draft,
    );
  };
  <div className="p-2">
    <form onSubmit className="space-y-2">
      <div> {React.string("Create a new Invoice:")} </div>
      <KsInvoiceFields invoice=None />
      <div>
        <button className=KsForm.submitClass disabled=isPending>
          {isPending
             ? <> {React.string("Creating ")} <KsSpinner /> </>
             : React.string("Create")}
        </button>
      </div>
      {switch (status) {
       | Succeeded(_) =>
         <div className={KsForm.bounce("bg-green-500")}>
           {React.string("Created!")}
         </div>
       | Failed(_) =>
         <div className={KsForm.bounce("bg-red-500")}>
           {React.string("Failed to create.")}
         </div>
       | Idle
       | Running(_) => React.null
       }}
    </form>
  </div>;
};
