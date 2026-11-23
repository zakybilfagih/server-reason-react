/* One HTTP client for code that runs on the server and in the browser. Paths
   are relative to the app: the browser resolves them against the page. */

type response = {
  status: int,
  ok: bool,
  body: string,
};

let request =
    (
      ~method_: [
         | `GET
         | `POST
       ]=`GET,
      ~body=?,
      ~signal=?,
      path,
    )
    : Js.Promise.t(response) =>
  Fetch.fetchWithInit(
    path,
    Fetch.RequestInit.make(
      ~method_=
        switch (method_) {
        | `GET => Fetch.Get
        | `POST => Fetch.Post
        },
      ~headers=Fetch.HeadersInit.make({ "Content-Type": "application/json" }),
      ~body=?Option.map(Fetch.BodyInit.make, body),
      ~signal?,
      (),
    ),
  )
  |> Js.Promise.then_(response =>
       Fetch.Response.text(response)
       |> Js.Promise.then_(body =>
            Js.Promise.resolve({
              status: Fetch.Response.status(response),
              ok: Fetch.Response.ok(response),
              body,
            })
          )
     );
