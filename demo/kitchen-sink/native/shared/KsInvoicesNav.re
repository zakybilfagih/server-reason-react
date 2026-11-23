/* InvoicesLayoutComponent */

let isPending = (status: RouterRuntime.Navigation.status, destination) =>
  switch (status) {
  | Loading({ to_, _ }) =>
    fst(KsLink.splitHref(RouterRuntime.href(destination))) == to_.pathname
  | Idle
  | Failed(_) => false
  };

[@react.client.component]
let make = (~children: React.element) => {
  let invoices =
    ReactQuery.useSuspenseQuery(KsData.invoicesQuery, ())
    |> ReactQuery.SuspenseResult.data;
  let (_, status) = Router.useNavigation();
  <div className="flex-1 flex">
    <div className="divide-y w-48">
      {invoices
       |> Array.map((invoice: KsData.invoice) => {
            let destination =
              Router.Invoice.destination(~invoiceId=invoice.id, ());
            <div key={Int.to_string(invoice.id)}>
              <KsLink destination className="block py-2 px-3 text-blue-700">
                <pre className="text-sm">
                  {React.string(
                     "#"
                     ++ Int.to_string(invoice.id)
                     ++ " - "
                     ++ Js.String.slice(~start=0, ~end_=10, invoice.title)
                     ++ " ",
                   )}
                  <KsSpinner
                    show={isPending(status, destination)}
                    wait="delay-50"
                  />
                </pre>
              </KsLink>
            </div>;
          })
       |> React.array}
    </div>
    <div className="flex-1 border-l"> children </div>
  </div>;
};
