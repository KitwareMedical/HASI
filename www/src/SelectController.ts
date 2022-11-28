import { ContextConsumer } from "@lit-labs/context";
import { LitElement, ReactiveController } from "lit";
import { StateFrom } from "xstate";
import { hasiContext, HasiMachine } from "./state/hasi.machine";

const defaultCompare = (a: any, b: any) => a === b;

export class SelectController<T> implements ReactiveController {
  host: LitElement;

  value: T | undefined;
  selector: (state: StateFrom<HasiMachine>) => T;
  compare: (a: T, b: T) => boolean; // is current value same as old value?

  serviceContext: ContextConsumer<typeof hasiContext, LitElement>;
  unsubscribe?: Function;

  constructor(
    host: LitElement,
    selector: (state: StateFrom<HasiMachine>) => T,
    compare: (a: T, b: T) => boolean = defaultCompare
  ) {
    (this.host = host).addController(this);
    this.serviceContext = new ContextConsumer(
      this.host,
      hasiContext,
      undefined,
      true
    );
    this.selector = selector;
    this.compare = compare;
  }

  onUpdate = (newState: StateFrom<HasiMachine>) => {
    const oldValue = this.value;
    this.value = this.selector(newState);
    if (oldValue === undefined || !this.compare(oldValue, this.value))
      this.host.requestUpdate();
  };

  hostConnected() {
    this.serviceContext.hostConnected();
    this.unsubscribe = this.serviceContext.value?.service.subscribe(
      this.onUpdate
    ).unsubscribe;
  }

  hostDisconnected() {
    if (this.unsubscribe) this.unsubscribe();
    this.serviceContext.hostDisconnected();
  }
}
