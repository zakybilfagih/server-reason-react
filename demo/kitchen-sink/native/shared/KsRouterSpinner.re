[@react.client.component]
let make = () => {
  let (_, status) = Router.useNavigation();
  let settings = KsSettings.use();
  <KsSpinner show={Option.is_some(KsPending.target(status, settings))} />;
};
