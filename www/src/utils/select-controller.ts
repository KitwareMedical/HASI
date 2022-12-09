import { ReactiveController, ReactiveControllerHost } from 'lit';
import { ActorRef, Subscribable, Subscription } from 'xstate';

function isActorWithState<T extends ActorRef<any>>(
  actorRef: T
): actorRef is T & { state: any } {
  return 'state' in actorRef;
}

export function getSnapshot<TEmitted>(
  actorRef: ActorRef<any, TEmitted>
): TEmitted | undefined {
  return 'getSnapshot' in actorRef
    ? actorRef.getSnapshot()
    : isActorWithState(actorRef)
    ? actorRef.state
    : undefined;
}

const defaultCompare = (a: any, b: any) => a === b;

export class SelectorController<
  TActor extends ActorRef<any, any>,
  T,
  TEmitted = TActor extends Subscribable<infer Emitted> ? Emitted : never
> implements ReactiveController
{
  private host: ReactiveControllerHost;
  private subscription: Subscription;
  private selected: T;

  constructor(
    host: ReactiveControllerHost,
    actorRef: TActor,
    selector: (emitted: TEmitted) => T,
    compare: (a: T, b: T) => boolean = defaultCompare
  ) {
    this.host = host;
    this.host.addController(this);

    this.selected = selector(getSnapshot(actorRef));

    this.subscription = actorRef.subscribe((emitted) => {
      const nextSelected = selector(emitted);
      if (!compare(this.selected, nextSelected)) {
        this.selected = nextSelected;
        this.host.requestUpdate();
      }
    });
  }

  get value() {
    return this.selected;
  }

  hostDisconnected() {
    this.subscription.unsubscribe();
  }
}

export function connectSelector<
  TActor extends ActorRef<any, any>,
  T,
  TEmitted = TActor extends Subscribable<infer Emitted> ? Emitted : never
>(
  host: ReactiveControllerHost,
  actorRef: TActor,
  selector: (emitted: TEmitted) => T,
  compare: (a: T, b: T) => boolean = defaultCompare
) {
  return new SelectorController(host, actorRef, selector, compare);
}
