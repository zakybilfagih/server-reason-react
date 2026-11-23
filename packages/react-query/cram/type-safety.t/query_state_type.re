open ReactQuery;

let count = client =>
  switch (QueryClient.getQueryState(client, Fixtures.note, 1)) {
  | Some({ data: Some(count), _ }) => count + 1
  | _ => 0
  };
