/* One HTTP client for code that runs on the server and in the browser. Paths
   are relative to the app: the browser resolves them against the page, and
   the server against [origin], which it sets at startup. */

type response = {
  status: int,
  ok: bool,
  body: string,
};

let origin = ref("http://localhost:8080");

/* Requests from the server say so, so an API can tell them apart. */
let userAgent = "server-reason-react";

let request =
    (
      ~method_: [
         | `GET
         | `POST
       ]=`GET,
      ~body=?,
      ~signal as _: option(Fetch.signal)=?,
      path,
    )
    : Js.Promise.t(response) => {
  let headers =
    Cohttp.Header.of_list([
      ("User-Agent", userAgent),
      ("Content-Type", "application/json"),
    ]);
  let%lwt (response, body) =
    Cohttp_lwt_unix.Client.call(
      ~headers,
      ~body=?Option.map(Cohttp_lwt.Body.of_string, body),
      (method_ :> Cohttp.Code.meth),
      Uri.of_string(origin^ ++ path),
    );
  let%lwt body = Cohttp_lwt.Body.to_string(body);
  let status = Cohttp.Code.code_of_status(Cohttp.Response.status(response));
  Lwt.return({
    status,
    ok: status >= 200 && status < 300,
    body,
  });
};
