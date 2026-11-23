open ReactQuery;

let reset = client =>
  QueryClient.setQueriesData(client, Fixtures.note, _ => Some(0));
