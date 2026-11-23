/* Saves and restores clients through a persister natively and in the
   browser build, which uses TanStack's persist-client functions, and logs
   what happens. persist.test.mjs checks both logs are the same. */
open ReactQuery;

let note =
  Query.make(
    ~name="persisted",
    ~key=(id: int) => [Key.int(id)],
    ~codec=
      Codec.make(
        ~encode=Melange_json.Primitives.string_to_json,
        ~decode=Melange_json.Primitives.string_of_json,
      ),
    ~fetch=(~signal as _, _) => Js.Promise.reject(Failure("not fetched")),
    (),
  );

let run = () => {
  let log = ref([]);
  let add = line => log := [line, ...log^];
  let saved = ref(None);
  let persister =
    Persister.make(
      ~persist=
        client => {
          saved := Some(client);
          Js.Promise.resolve();
        },
      ~restore=() => Js.Promise.resolve(saved^),
      ~remove=
        () => {
          add("removed");
          saved := None;
          Js.Promise.resolve();
        },
    );
  let restore = (~maxAge=?, ~buster=?, client) =>
    persistQueryClientRestore(~client, ~persister, ~maxAge?, ~buster?, ())
    |> Js.Promise.then_(() => {
         add(
           "data "
           ++ Option.value(
                QueryClient.getQueryData(client, note, 1),
                ~default="none",
              ),
         );
         Js.Promise.resolve();
       });
  let saving = QueryClient.make();
  QueryClient.setQueryData(saving, note, 1, _ => Some("saved"));
  let broken =
    Persister.make(
      ~persist=_ => Js.Promise.resolve(),
      ~restore=() => Js.Promise.reject(Failure("unreadable")),
      ~remove=
        () => {
          add("broken removed");
          Js.Promise.resolve();
        },
    );

  persistQueryClientSave(~client=saving, ~persister, ~buster="v1", ())
  |> Js.Promise.then_(() => {
       add(
         "saved with buster "
         ++ Option.fold(
              ~none="nothing",
              ~some=(client: Persister.persistedClient) => client.buster,
              saved^,
            ),
       );
       restore(~buster="v1", QueryClient.make());
     })
  |> Js.Promise.then_(() => {
       let newer = QueryClient.make();
       QueryClient.setQueryData(newer, note, 1, _ => Some("newer"));
       restore(~buster="v1", newer);
     })
  |> Js.Promise.then_(() => restore(~buster="v2", QueryClient.make()))
  |> Js.Promise.then_(() => restore(~buster="v2", QueryClient.make()))
  |> Js.Promise.then_(() =>
       persistQueryClientSave(~client=saving, ~persister, ())
     )
  |> Js.Promise.then_(() => restore(~maxAge=-1., QueryClient.make()))
  |> Js.Promise.then_(() => {
       saved :=
         Some({
           Persister.timestamp: 0.,
           buster: "",
           clientState: dehydrate(saving),
         });
       restore(~maxAge=infinity, QueryClient.make());
     })
  |> Js.Promise.then_(() =>
       persistQueryClientRestore(
         ~client=QueryClient.make(),
         ~persister=broken,
         (),
       )
       |> Js.Promise.then_(() => {
            add("restored");
            Js.Promise.resolve();
          })
       |> Js.Promise.catch(_ => {
            add("restore failed");
            Js.Promise.resolve();
          })
     )
  |> Js.Promise.then_(() =>
       Js.Promise.resolve(Array.of_list(List.rev(log^)))
     );
};
