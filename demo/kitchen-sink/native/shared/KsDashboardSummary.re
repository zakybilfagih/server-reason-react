[@react.client.component]
let make = () => {
  let invoices =
    ReactQuery.useSuspenseQuery(KsData.invoicesQuery, ())
    |> ReactQuery.SuspenseResult.data;
  <div className="p-2">
    <div className="p-2">
      {React.string("Welcome to the dashboard! You have ")}
      <strong>
        {React.string(
           Int.to_string(Array.length(invoices)) ++ " total invoices",
         )}
      </strong>
      {React.string(".")}
    </div>
  </div>;
};
