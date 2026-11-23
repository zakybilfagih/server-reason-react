  $ compile() { ocamlfind ocamlc -c -w +8 -warn-error +8 -package server-reason-react.react-query-native,melange-json-native -pp "refmt --print binary" -impl "$1"; }
  $ compile fixtures.re
  $ compile good.re

The key has the query's key type.

  $ compile wrong_key.re
  File "wrong_key.re", line 1, characters 46-49:
  1 | let data = ReactQuery.useQuery(Fixtures.note, "1");
                                                    ^^^
  Error: This constant has type string but an expression was expected of type
           int
  [2]

[select] changes the data type.

  $ compile select_type.re
  File "select_type.re", line 5, characters 22-27:
  5 |   | Success(title) => title ++ "!"
                            ^^^^^
  Error: The value title has type int but an expression was expected of type
           string
  [2]

A status match covers every case.

  $ compile missing_case.re
  File "missing_case.re", lines 4-7, characters 2-3:
  4 | ..switch (QueryResult.status(useQuery(Fixtures.note, 1))) {
  5 |   | Success(title) => title
  6 |   | Pending => "loading"
  7 |   }.
  Error (warning 8 [partial-match]): this pattern-matching is not exhaustive.
    Here is an example of a case that is not matched: Error _
  [2]

A result is read with accessors.

  $ compile result_is_abstract.re
  File "result_is_abstract.re", line 5, characters 2-18:
  5 |   | Success(title) => title
        ^^^^^^^^^^^^^^^^
  Error: This pattern matches values of type 'a ReactQuery.status
         but a pattern was expected which matches values of type
           string ReactQuery.QueryResult.t
  [2]

The codec has the type of the data the query fetches.

  $ compile codec_type.re
  File "codec_type.re", line 12, characters 33-46:
  12 |     ~fetch=(~signal as _, ()) => Lwt.return(1),
                                        ^^^^^^^^^^^^^
  Error: This expression has type int Lwt.t
         but an expression was expected of type
           string Js.Promise.t = string Lwt.t
         Type int is not compatible with type string
  [2]

A mutation's callbacks get back what [onMutate] returned.

  $ compile mutation_context.re
  File "mutation_context.re", line 8, characters 80-88:
  8 |     ~onError=(_, _, _, previous) => Lwt.return(ignore(Option.map(String.length, previous))),
                                                                                      ^^^^^^^^
  Error: The value previous has type int option
         but an expression was expected of type string option
         Type int is not compatible with type string
  [2]

[setQueryData] keeps the query's data type.

  $ compile set_query_data_type.re
  File "set_query_data_type.re", line 4, characters 63-64:
  4 |   QueryClient.setQueryData(client, Fixtures.note, 1, _ => Some(0));
                                                                     ^
  Error: The constant 0 has type int but an expression was expected of type
           string
  [2]

A mutation's per-call callbacks get its data type.

  $ compile mutate_callback_type.re
  File "mutate_callback_type.re", line 7, characters 4-10:
  7 |     rename,
          ^^^^^^
  Error: The value rename has type
           (int * string, int) ReactQuery.MutationResult.t
         but an expression was expected of type
           (int * string, string) ReactQuery.MutationResult.t
         Type int is not compatible with type string
  [2]

[useMutationState] gives the mutation's variables type.

  $ compile mutation_state_type.re
  File "mutation_state_type.re", line 4, characters 2-35:
  4 |   useMutationState(Fixtures.rename)
        ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  Error: This expression has type
           (int * string, int) ReactQuery.mutationStatus array
         but an expression was expected of type
           (string, 'a) ReactQuery.mutationStatus array
         Type int * string is not compatible with type string
  [2]

[setQueriesData] keeps the query's data type.

  $ compile set_queries_data_type.re
  File "set_queries_data_type.re", line 4, characters 62-63:
  4 |   QueryClient.setQueriesData(client, Fixtures.note, _ => Some(0));
                                                                    ^
  Error: The constant 0 has type int but an expression was expected of type
           string
  [2]

[getQueryState] has the query's data type.

  $ compile query_state_type.re
  File "query_state_type.re", line 5, characters 38-43:
  5 |   | Some({ data: Some(count), _ }) => count + 1
                                            ^^^^^
  Error: The value count has type string but an expression was expected of type
           int
  [2]

The next page param has the type of [initialPageParam].

  $ compile page_param_type.re
  File "page_param_type.re", line 19, characters 32-38:
  19 |     ~getNextPageParam=_ => Some("next"),
                                       ^^^^^^
  Error: This constant has type string but an expression was expected of type
           int
  [2]

Pages have the type [fetchPage] returns.

  $ compile page_type.re
  File "page_type.re", line 5, characters 53-58:
  5 |   | Success({ pages, _ }) => Array.fold_left((+), 0, pages)
                                                           ^^^^^
  Error: The value pages has type string array
         but an expression was expected of type int array
         Type string is not compatible with type int
  [2]

Each query in [SuspenseQueries.both] keeps its data type.

  $ compile queries_type.re
  File "queries_type.re", line 8, characters 2-28:
  8 |   SuspenseResult.data(first) + SuspenseResult.data(second);
        ^^^^^^^^^^^^^^^^^^^^^^^^^^
  Error: This expression has type string but an expression was expected of type
           int
  [2]

The reducer takes the stream's chunk type.

  $ compile reducer_type.re
  File "reducer_type.re", line 13, characters 39-44:
  13 |     ~reducer=(total, chunk) => total + chunk,
                                              ^^^^^
  Error: The value chunk has type string but an expression was expected of type
           int
  [2]
