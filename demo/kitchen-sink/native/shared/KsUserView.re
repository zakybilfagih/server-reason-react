/* UserComponent */
[@react.client.component]
let make = (~userId: int) => {
  let user =
    ReactQuery.useSuspenseQuery(KsData.userQuery, userId)
    |> ReactQuery.SuspenseResult.data;
  <>
    <h4 className="p-2 font-bold"> {React.string(user.name)} </h4>
    <pre className="text-sm whitespace-pre-wrap">
      {React.string(KsData.stringifyUser(user))}
    </pre>
  </>;
};
