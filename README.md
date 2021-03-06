![nact Logo](https://raw.githubusercontent.com/ncthbrt/nact/master/assets/logo.svg?sanitize=true)

**let reason-nact = (node.js, reason, actors) ⇒ your µ services have never been so typed**


<!-- Badges -->
[![Travis branch](https://img.shields.io/travis/ncthbrt/reason-nact/master.svg?style=flat-square)]()
[![Coveralls](https://img.shields.io/coveralls/ncthbrt/reason-nact.svg?style=flat-square)]() [![Dependencies](https://david-dm.org/ncthbrt/nact.svg?branch=master&style=flat-square)](https://david-dm.org/ncthbrt/reason-nact) 

[![npm](https://img.shields.io/npm/v/nact.svg?style=flat-square)](https://www.npmjs.com/package/reason-nact) [![we are reactive](https://img.shields.io/badge/we_are-reactive-blue.svg?style=flat-square)](https://www.reactivemanifesto.org/)

> Note:
>
> Any and all feedback, comments and suggestions are welcome. Please open an issue if you
> find anything unclear or misleading in the documentation. 

# Sponsored by 
[![Root Logo](https://raw.githubusercontent.com/ncthbrt/nact/master/root-logo.svg?sanitize=true)](https://root.co.za)

# Table of Contents
  * [Introduction](#introduction)
  * [Core Concepts](#core-concepts)
    * [Getting Started](#getting-started)
    * [Stateful Actors](#stateful-actors)
    * [Actor Communication](#actor-communication)
    * [Querying](#querying)
    * [Hierarchy](#hierarchy)
    * [Persistence](#persistence)
      * [Snapshotting](#snapshotting)
      * [Timeouts](#timeouts)  
<!--  * Patterns and Practises !-->

# Introduction

Nact is an implementation of the actor model for Node.js. It is inspired by the approaches taken by [Akka](getakka.net) and [Erlang](https://erlang.com). Additionally it attempts to provide a familiar interface to users coming from Redux. This project provides a wrapper around nact for those using ReasonML and/or Bucklescript.

The goal of the project is to provide a simple way to create and reason about µ-services and asynchronous event driven architectures in Node.js.

The actor model describes a system made up of a set of entities called actors. An actor could be described as an independently running packet of state. Actors communicate solely by passing messages to one another.  Actors can also create more (child) actors.

Actor systems have been used to drive hugely scalable and highly available systems (such as WhatsApp and Twitter), but that doesn't mean it is exclusively for companies with big problems and even bigger pockets. Architecting a system using actors should be an option for any developer considering considering a move to a µ-services architecture:

  * Creating a new type of actor is a very lightweight operation in contrast to creating a whole new microservice.
  * [Location transparency](https://doc.akka.io/docs/akka/2.5.4/java/general/remoting.html) and no shared state mean that it is possible to defer decisions around where to deploy a subsystem, avoiding the commonly cited problem of prematurely choosing a [bounded context](https://vimeo.com/74589816).
  * Using actors mean that the spaghetti you might see in a monolithic system is far less likely to happen in the first place as message passing creates less coupled systems. 
  * Actors are asynchronous by design and closely adhere to the principles enumerated in the [reactive manifesto](https://www.reactivemanifesto.org/)
  * Actors deal well with both state and statelessness, so creating a smart cache, an in-memory event store or a stateful worker is just as easy as creating a stateless db repository layer without increasing infrastructural complexity.

## Caveats

While network transparency and clustering are planned features of the framework, they have not been implemented yet.

# Core Concepts

## Getting Started

Nact has only been tested to work on Node 8 and above. You can install nact in your project by invoking the following:

```bash
npm install --save reason-nact
```


You'll also need to add `reason-nact` to your dependencies in the `bsconfig.json` file.

Once installed, you need to import the start function, which starts and then returns the actor system.

```reason
open Nact;
let system = start();
```

Once you have a reference to the system, it is now possible to create our first actor. To create an actor you have to `spawn` it.  As is traditional, let us create an actor which says hello when a message is sent to it. Since this actor doesn't require any state, we can use the simpler `spawnStateless` function.

```reason
type greetingMsg = {name: string};

let greeter =
  spawnStateless(
    ~name="greeter",
    system,
    ({name}, _) => print_endline("Hello " ++ name) |> Js.Promise.resolve
  );

dispatch(greeter, {name: "Erlich Bachman"});
```

The first unamed argument to `spawnStateless` is the parent, which is in this case the actor system. The [hierarchy](#hierarchy) section will go into more detail about this.

The second unamed argument to `spawnStateless` is a function which is invoked when a message is received.

The name argument to `spawnStateless` is optional, and if omitted, the actor is automatically assigned a name by the system.

To communicate with the greeter, we need to `dispatch` a message to it informing it who we are:

```reason
dispatch(greeter, { name: "Erlich Bachman" });
```

This should print `Hello Erlich Bachman` to the console. 

> Note: Stateless actors can service multiple requests at the same time. Statelessness means that such actors do not have to cater for concurrency issues.

To complete this example, we need to shutdown our system. We can do this by calling `stop(system)`

An alternative to calling dispatch is opening `Nact.Operators` and using the  `<-<` operator:

```reason
open Nact.Operators;
greeter <-< { name: "Erlich Bachman" };
{ name: "Erlich Bachman" } >-> greeter;
```

## Stateful Actors

One of the major advantages of an actor system is that it offers a safe way of creating stateful services. A stateful actor is created using the `spawn` function.

In this example, the state is initialized to an empty object. Each time a message is received by the actor, the current state is passed in as the first argument to the actor.  Whenever the actor encounters a name it hasn't encountered yet, it returns a copy of previous state with the name added. If it has already encountered the name it simply returns the unchanged current state. The return value is used as the next state.

```reason
let statefulGreeter =
  spawn(
    ~name="stateful-greeter",
    system,
    (state, {name}, ctx) => {
      let hasPreviouslyGreetedMe = List.exists((v) => v === name, state);
      if (hasPreviouslyGreetedMe) {
        print_endline("Hello again " ++ name);
        state |> Js.Promise.resolve;
      } else {
        print_endline("Good to meet you, " ++ name ++ ". I am the " ++ ctx.name ++ " service!");
        [name, ...state] |> Js.Promise.resolve;
      }
    },
    []
  );
```

## Actor Communication

An actor alone is a somewhat useless construct; actors need to work together. Actors can send messages to one another by using the `dispatch` method. 

In this example, the actors Ping and Pong are playing a perfect ping-pong match. To start the match, we dispatch a message to ping and 
specify that the sender in msgType is pong.


```reason
open Nact;

open Nact.Operators;

let system = start();

type msgType =
  | Msg(actorRef(msgType), string);

let ping: actorRef(msgType) =
  spawnStateless(
    ~name="ping",
    system,
    (Msg(sender, msg), ctx) => {
      print_endline(msg);
      sender <-< Msg(ctx.self, ctx.name) |> Js.Promise.resolve
    }
  );

let pong: actorRef(msgType) =
  spawnStateless(
    ~name="pong",
    system,
    (Msg(sender, msg), ctx) => {
      print_endline(msg);
      sender <-< Msg(ctx.self, ctx.name) |> Js.Promise.resolve
    }
  );

ping <-< Msg(pong, "hello");

```
This produces the following console output:

``` 
begin
ping
pong
ping
pong
ping
...
```

## Querying

Actor systems don't live in a vacuum, they need to be available to the outside world. Commonly actor systems are fronted by REST APIs or RPC frameworks. REST and RPC style access patterns are blocking: a request comes in, it is processed, and finally returned to the sender using the original connection. To help bridge nact's non blocking nature, Nact provides a `query` function. Query returns a promise.

Similar to `dispatch`, `query` pushes a message on to an actor's mailbox, but differs in that it also creates a temporary actor. The temporary actor is passed into a function which returns the message to send to the target actor. When the temporary actor receives a message, the promise returned by the query resolves. 

In addition to the message, `query` also takes in a timeout value measured in milliseconds. If a query takes longer than this time to resolve, it times out and the promise is rejected. A time bounded query is very important in a production system; it ensures that a failing subsystem does not cause cascading faults as queries queue up and stress available system resources.

In this example, we'll create a simple single user in-memory address book system.

> Note: We'll expand on this example in later sections.

What are the basic requirements of a basic address book API? It should be able to:
 - Create a new contact 
 - Fetch all contacts
 - Fetch a specific contact
 - Update an existing contact
 - Delete a contact

Because actor are message driven, let us define the message types used between the api and actor system:

```reason
type contactId =
  | ContactId(int);

type contact = {
  name: string,
  email: string
};

type contactResponseMsg =
  | Success(contact)
  | NotFound;

type contactMsg =
  | CreateContact(contact)
  | RemoveContact(contactId)
  | UpdateContact(contactId, contact)
  | FindContact(contactId);
```
We also need to describe the shape of the contact actor's state. In this example, it was decided to create a `ContactIdMap` map to hold the list of contacts. `seqNumber` is used to assign each contact a unique identifier. `seqNumber` monotonically increases, even if a contact is deleted:

```reason
module ContactIdCompare = {
  type t = contactId;
  let compare = (ContactId(left), ContactId(right)) => compare(left, right);
};

module ContactIdMap = Map.Make(ContactIdCompare);

type contactsServiceState = {
  contacts: ContactIdMap.t(contact),
  seqNumber: int
};
```

Now let us create functions to handle each message type:

```reason
let createContact = ({contacts, seqNumber}, sender, contact) => {
  let contactId = ContactId(seqNumber);
  sender <-< (contactId, Success(contact));
  let nextContacts = ContactIdMap.add(contactId, contact, contacts);
  {contacts: nextContacts, seqNumber: seqNumber + 1}
};

let removeContact = ({contacts, seqNumber}, sender, contactId) => {
  let nextContacts = ContactIdMap.remove(contactId, contacts);
  let msg =
    if (contacts === nextContacts) {
      let contact = ContactIdMap.find(contactId, contacts);
      (contactId, Success(contact))
    } else {
      (contactId, NotFound)
    };
  sender <-< msg;
  {contacts: nextContacts, seqNumber}
};

let updateContact = ({contacts, seqNumber}, sender, contactId, contact) => {
  let nextContacts =
    ContactIdMap.remove(contactId, contacts) |> ContactIdMap.add(contactId, contact);
  let msg =
    if (nextContacts === contacts) {
      (contactId, Success(contact))
    } else {
      (contactId, NotFound)
    };
  sender <-< msg;
  {contacts: nextContacts, seqNumber}
};

let findContact = ({contacts, seqNumber}, sender, contactId) => {
  let msg =
    try (contactId, Success(ContactIdMap.find(contactId, contacts))) {
    | Not_found => (contactId, NotFound)
    };
  sender <-< msg;
  {contacts, seqNumber}
};
```
Finally we can put it all together and create the actor:

```reason
let system = start();

let contactsService =
  spawn(
    ~name="contacts",
    system,
    (state, (sender, msg), _) =>
      (
        switch msg {
        | CreateContact(contact) => createContact(state, sender, contact)
        | RemoveContact(contactId) => removeContact(state, sender, contactId)
        | UpdateContact(contactId, contact) => updateContact(state, sender, contactId, contact)
        | FindContact(contactId) => findContact(state, sender, contactId)
        }
      )
      |> Js.Promise.resolve,
    {contacts: ContactIdMap.empty, seqNumber: 0}
  );
```

This should leave you with a working but very basic contacts service. 
We can now interact with this actor from outside the actor system by calling the query function. In the example below, we are passing in a 
function which constructs the final message to sender to the contactsService actor:

```reason
let createDinesh = query(
    ~timeout=100 * milliseconds,
    contactsService,
    (tempReference) => (
      tempReference,
      CreateContact({name: "Dinesh Chugtai", email: "dinesh@piedpiper.com"})
    )
  );
```

## Hierarchy

The application we made in the [querying](#querying) section isn't very useful. For one it only supports a single user's contacts, and secondly it forgets all the user's contacts whenever the system restarts. In this section we'll solve the multi-user problem by exploiting an important feature of any blue-blooded actor system: the hierachy.

Actors are arranged hierarchially, they can create child actors of their own, and accordingly every actor has a parent. The lifecycle of an actor is tied to its parent; if an actor stops, then it's children do too.

Up till now we've been creating actors which are children of the actor system (which is a pseudo actor). However in a real system, this would be considered an anti pattern, for much the same reasons as placing all your code in a single file is an anti-pattern. By exploiting the actor hierarchy, you can enforce a separation of concerns and encapsulate system functionality, while providing a coherent means of reasoning with failure and system shutdown. 

Let us imagine that the single user contacts service was simple a part of some larger system; an email campaign management API for example.  A potentially valid system could perhaps be represented by the diagram below. 

<img height="500px" alt="Example of an Actor System Hierarchy" src="https://raw.githubusercontent.com/ncthbrt/nact/master/assets/hierarchy-diagram.svg?sanitize=true"/>

In the diagram, the email service is responsible for managing the template engine and email delivery, while the contacts service has choosen to model each user's contacts as an actor. (This is a very feasible approach in production provided you shutdown actors after a period of inactivity)

Let us focus on the contacts service to see how we can make effective of use of the hierarchy. To support multiple users, we need do three things: 

- Modify our original contacts service to so that we can parameterise its parent and name
- Create a parent to route requests to the correct child
- Add a user id to the path of each API endpoint and add a `userId` into each message.

Modifying our original service is as simple as the following:

```reason
let createContactsService = (parent, userId) =>
  spawn(
    ~name=userId,
    parent,
    (state, (sender, msg), _) =>
      (
        switch msg {
        | CreateContact(contact) => createContact(state, sender, contact)
        | RemoveContact(contactId) => removeContact(state, sender, contactId)
        | UpdateContact(contactId, contact) => updateContact(state, sender, contactId, contact)
        | FindContact(contactId) => findContact(state, sender, contactId)
        }
      )
      |> Js.Promise.resolve,
    {contacts: ContactIdMap.empty, seqNumber: 0}
  );
```

Now we need to create the parent contact service. The parent checks if it has a child with the userId as the key. If it does not, it spawns the 
child actor:

```reason

let contactsService =
  spawn(
    system,
    (children, (sender, userId, msg), ctx) => {
      let potentialChild =
        try (Some(StringMap.find(userId, children))) {
        | _ => None
        };
      Js.Promise.resolve(
        switch potentialChild {
        | Some(child) =>
          dispatch(child, (sender, msg));
          children
        | None =>
          let child = createContactsService(ctx.self, userId);
          dispatch(child, (sender, msg));
          StringMap.add(userId, child, children)
        }
      )
    },
    StringMap.empty
  );
```

These two modifications show the power of an actor hierarchy. The contact service doesn't need to know the implementation details of its children (and doesn't even have to know about what kind of messages the children can handle). The children also don't need to worry about multi tenancy and can focus on the domain.

Now the only thing remaining for a MVP of our contacts service is some way of persisting changes...

## Persistence

The contacts service we've been working on *still* isn't very useful. While we've extended the service to support multiple users, it has the unfortunate limitation that it loses the contacts each time the program restarts. To remedy this, nact extends stateful actors by adding a new function: `persist` 

To use `persist`, the first thing we need to do is specify a persistence engine. Currently only a [PostgreSQL](https://github.com/ncthbrt/reason-nact-postgres) engine is available (though it should be easy to create your own). To work with the PostgreSQL engine, install the persistent provider package using the command `npm install --save reason-nact-postgres`. Also ensure you add
the package to `bsconfig.json`. Now we'll need to modify the code creating the system to look something like the following (replacing "CONNECTION_STRING" with a valid postgresql connection string of course):

```reason
let system = start(~persistenceEngine=NactPostgres.create("CONNECTION_STRING"), ());
```

The optional parameter `~persistenceEngine` adds the persistence plugin to the system using the specified persistence engine.

Now the only remaining work is to modify the contacts service to allow persistence. When the actor start up, it first receives all the persisted messages and then can begin processing new ones. 

```reason
let createContactsService = (parent, userId) =>
  spawnPersistent(
    ~key="contacts" ++ userId,
    ~name=userId,
    parent,
    (state, (sender, msg), {persist}) =>
      persist((sender, msg))
      |> Js.Promise.then_ (
        () =>
          (
            switch msg {
            | CreateContact(contact) => createContact(state, sender, contact)
            | RemoveContact(contactId) => removeContact(state, sender, contactId)
            | UpdateContact(contactId, contact) => updateContact(state, sender, contactId, contact)
            | FindContact(contactId) => findContact(state, sender, contactId)
            }
          )
          |> Js.Promise.resolve
      ),
    {contacts: ContactIdMap.empty, seqNumber: 0}
  );
```

The `~key` parameter supplied when spawning the persistent actor is very important and should be a unique value. The key is used to save and retrieve snapshots and persisted events.

### Snapshotting

Sometimes actors accumulate a lot of persisted events. This is problematic because it means that it can take a potentially long time for an actor to recover. For time-sensitive applictions, this would make nact an unsuitable proposition. Snapshotting is a way to skip replaying every single event. When a persistent actor starts up again, nact checks to see if there are any snapshots available in the *snapshot store*. Nact selects the latest snapshot. The snapshot contains the sequence number at which it was taken. The snapshot is passed as the initial state to the actor, and only the events which were persisted after the snapshot are replayed. 

To modify the user contacts service to support snapshotting, we refactor the code to the following:

```reason
let createContactsService = (parent, userId) =>
  spawnPersistent(
    ~key="contacts" ++ userId,
    ~name=userId,
    ~snapshotEvery=10 * messages,
    parent,
    (state, (sender, msg), {persist}) => {
      /* Same function as before */
    }    
    {contacts: ContactIdMap.empty, seqNumber: 0}
  );
```
Here we are using the optional argument `snapshotEvery` to instruct nact to take a snapshot every 10 messages.

### Timeouts

While not strictly a part of the persistent actor, timeouts are frequently used with snapshotting. Actors take up memory, which is still a limited resource. If an actor has not processed messages in a while, it makes sense to shut it down until it is again needed; this frees up memory. Adding a timeout to the user contacts service is similar to snapshotting:

```reason
let createContactsService = (parent, userId) =>
  spawnPersistent(
    ~key="contacts" ++ userId,
    ~name=userId,
    ~shutdownAfter=15 * minutes,
    ~snapshotEvery=10 * messages,
    parent,
    (state, (sender, msg), {persist}) => {
      /* Same function as before */
    }    
    {contacts: ContactIdMap.empty, seqNumber: 0}
  );
```
