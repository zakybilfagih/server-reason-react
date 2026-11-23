open ReactQuery;

let reset = client =>
  QueryClient.setQueryData(client, Fixtures.note, 1, _ => Some(0));
