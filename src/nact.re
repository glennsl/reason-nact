module StringSet = Nact_stringSet;

type persistenceEngine = Nact_bindings.persistenceEngine;

type actorPath =
  | ActorPath(Nact_bindings.actorPath);

type actorRef('msg) =
  | ActorRef(Nact_bindings.actorRef);

type ctx('msg, 'parentMsg) = {
  parent: actorRef('parentMsg),
  path: actorPath,
  self: actorRef('msg),
  children: StringSet.t,
  name: string
};

type persistentCtx('msg, 'parentMsg) = {
  parent: actorRef('parentMsg),
  path: actorPath,
  self: actorRef('msg),
  name: string,
  persist: 'msg => Js.Promise.t(unit),
  children: StringSet.t,
  recovering: bool
};

let mapCtx = (untypedCtx: Nact_bindings.ctx) => {
  name: untypedCtx##name,
  self: ActorRef(untypedCtx##self),
  parent: ActorRef(untypedCtx##parent),
  path: ActorPath(untypedCtx##path),
  children: untypedCtx##children |> Nact_jsMap.keys |> StringSet.fromJsArray
};

let mapPersist = (persist, msg) => persist(msg);

let mapPersistentCtx = (untypedCtx: Nact_bindings.persistentCtx('incoming)) => {
  name: untypedCtx##name,
  self: ActorRef(untypedCtx##self),
  parent: ActorRef(untypedCtx##parent),
  path: ActorPath(untypedCtx##path),
  recovering:
    switch (untypedCtx##recovering |> Js.Nullable.to_opt) {
    | Some(b) => b
    | None => false
    },
  persist: mapPersist(untypedCtx##persist),
  children: untypedCtx##children |> Nact_jsMap.keys |> StringSet.fromJsArray
};

type statefulActor('state, 'msg, 'parentMsg) =
  ('state, 'msg, ctx('msg, 'parentMsg)) => Js.Promise.t('state);

type statelessActor('msg, 'parentMsg) = ('msg, ctx('msg, 'parentMsg)) => Js.Promise.t(unit);

type persistentActor('state, 'msg, 'parentMsg) =
  ('state, 'msg, persistentCtx('msg, 'parentMsg)) => Js.Promise.t('state);

let spawn = (~name=?, ~shutdownAfter=?, ActorRef(parent), func, initialState) => {
  let options = {"shutdownAfter": Js.Nullable.from_opt(shutdownAfter)};
  let f = (possibleState, msg, ctx) => {
    let state =
      switch (Js.Nullable.to_opt(possibleState)) {
      | None => initialState
      | Some(concreteState) => concreteState
      };
    func(state, msg, mapCtx(ctx))
  };
  let untypedRef =
    switch name {
    | Some(concreteName) =>
      Nact_bindings.spawn(parent, f, Js.Nullable.return(concreteName), options)
    | None => Nact_bindings.spawn(parent, f, Js.Nullable.undefined, options)
    };
  ActorRef(untypedRef)
};

let spawnStateless = (~name=?, ~shutdownAfter=?, ActorRef(parent), func) => {
  let options = {"shutdownAfter": Js.Nullable.from_opt(shutdownAfter)};
  let f = (msg, ctx) => func(msg, mapCtx(ctx));
  let untypedRef =
    switch name {
    | Some(concreteName) =>
      Nact_bindings.spawnStateless(parent, f, Js.Nullable.return(concreteName), options)
    | None => Nact_bindings.spawnStateless(parent, f, Js.Nullable.undefined, options)
    };
  ActorRef(untypedRef)
};

let spawnPersistent =
    (~key, ~name=?, ~shutdownAfter=?, ~snapshotEvery=?, ActorRef(parent), func, initialState) => {
  let options: Nact_bindings.persistentActorOptions = {
    "shutdownAfter": Js.Nullable.from_opt(shutdownAfter),
    "snapshotEvery": Js.Nullable.from_opt(snapshotEvery)
  };
  let f = (possibleState, msg, ctx) => {
    let state =
      switch (Js.Null_undefined.to_opt(possibleState)) {
      | None => initialState
      | Some(concreteState) => concreteState
      };
    func(state, msg, mapPersistentCtx(ctx))
  };
  let untypedRef =
    switch name {
    | Some(concreteName) =>
      Nact_bindings.spawnPersistent(parent, f, key, Js.Nullable.return(concreteName), options)
    | None => Nact_bindings.spawnPersistent(parent, f, key, Js.Nullable.undefined, options)
    };
  ActorRef(untypedRef)
};

let stop = (ActorRef(reference)) => Nact_bindings.stop(reference);

type systemMsg;

let start = (~persistenceEngine=?, ()) => {
  let untypedRef =
    switch persistenceEngine {
    | Some(engine) => Nact_bindings.start([|Nact_bindings.configurePersistence(engine)|])
    | None => Nact_bindings.start([||])
    };
  ActorRef(untypedRef)
};

let dispatch = (ActorRef(recipient), msg) => Nact_bindings.dispatch(recipient, msg);

exception QueryTimeout(int);

let query = (~timeout: int, ActorRef(recipient), msgF) => {
  let f = (tempReference) => msgF(ActorRef(tempReference));
  Nact_bindings.query(recipient, f, timeout)
  |> Js.Promise.catch((_) => Js.Promise.reject(QueryTimeout(timeout)))
};

let milliseconds = 1;

let millisecond = milliseconds;

let seconds = 1000 * milliseconds;

let second = seconds;

let minutes = 60 * seconds;

let minute = minutes;

let hours = 60 * minutes;

let messages = 1;

let message = 1;

module Operators = {
  let (<-<) = (actorRef, msg) => dispatch(actorRef, msg);
  let (>->) = (msg, actorRef) => dispatch(actorRef, msg);
};