module type S = {
  type queryClient;

  /** Puts the queries the server streamed in the client's cache: the ones
      already in the document, then each one as it arrives. Call it before
      [hydrateRoot]. */
  let hydrateFromServer: queryClient => unit;
};
