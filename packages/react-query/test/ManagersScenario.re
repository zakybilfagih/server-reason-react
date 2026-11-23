/* Drives the focus and online managers through the same steps natively and
   in the browser build, which uses TanStack's own managers, and logs what
   happens. managers.test.mjs checks both logs are the same. */
open ReactQuery;

let run = () => {
  let log = ref([]);
  let add = line => log := [line, ...log^];
  let flag = (name, value) => add(name ++ " " ++ string_of_bool(value));

  flag("focused", FocusManager.isFocused());
  let stopA = FocusManager.subscribe(flag("a"));
  FocusManager.setFocused(Some(false));
  FocusManager.setFocused(Some(false));
  flag("focused", FocusManager.isFocused());
  FocusManager.setFocused(None);
  FocusManager.setEventListener(setFocused => {
    add("setup");
    setFocused(Some(false));
    setFocused(None);
    Some(() => add("cleanup"));
  });
  let stopB = FocusManager.subscribe(flag("b"));
  FocusManager.setFocused(Some(true));
  stopA();
  stopB();
  let stopC = FocusManager.subscribe(flag("c"));
  FocusManager.setEventListener(_ => {
    add("setup without cleanup");
    None;
  });
  let stopD = FocusManager.subscribe(flag("d"));
  stopC();
  stopD();
  flag("focused", FocusManager.isFocused());
  FocusManager.setFocused(None);
  flag("focused", FocusManager.isFocused());

  flag("online", OnlineManager.isOnline());
  let stopE = OnlineManager.subscribe(flag("e"));
  OnlineManager.setOnline(false);
  OnlineManager.setOnline(false);
  OnlineManager.setOnline(true);
  OnlineManager.setEventListener(setOnline => {
    add("online setup");
    setOnline(false);
    Some(() => add("online cleanup"));
  });
  flag("online", OnlineManager.isOnline());
  stopE();
  let stopF = OnlineManager.subscribe(flag("f"));
  OnlineManager.setOnline(true);
  stopF();

  NotifyManager.setScheduler(callback => {
    add("scheduled");
    callback();
  });
  NotifyManager.setNotifyFunction(callback => {
    add("notify");
    callback();
  });
  NotifyManager.setBatchNotifyFunction(callback => {
    add("batch start");
    callback();
    add("batch end");
  });
  NotifyManager.schedule(() => add("alone"));
  let result =
    NotifyManager.batch(() => {
      NotifyManager.schedule(() => add("first"));
      NotifyManager.batch(() => NotifyManager.schedule(() => add("nested")));
      add("inner batch done");
      NotifyManager.batchCalls(value => add("called " ++ value), "later");
      42;
    });
  add("batch returned " ++ Int.to_string(result));
  NotifyManager.batch(() => ());
  add("empty batch done");
  NotifyManager.setScheduler(NotifyManager.defaultScheduler);
  NotifyManager.setNotifyFunction(callback => callback());
  NotifyManager.setBatchNotifyFunction(callback => callback());

  Array.of_list(List.rev(log^));
};
