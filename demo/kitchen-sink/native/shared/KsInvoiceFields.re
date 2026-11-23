[@react.component]
let make = (~invoice: option(KsData.invoice), ~disabled=false) =>
  <div className="space-y-2">
    <h2 className="font-bold text-lg">
      <input
        name="title"
        defaultValue=?{
          Option.map((invoice: KsData.invoice) => invoice.title, invoice)
        }
        placeholder="Invoice Title"
        className="border border-opacity-50 rounded-sm p-2 w-full"
        disabled
      />
    </h2>
    <div>
      <textarea
        name="body"
        defaultValue=?{
          Option.map((invoice: KsData.invoice) => invoice.body, invoice)
        }
        rows=6
        placeholder="Invoice Body..."
        className="border border-opacity-50 p-2 rounded-sm w-full"
        disabled
      />
    </div>
  </div>;
