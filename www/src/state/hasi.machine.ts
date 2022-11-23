import { createContext } from "@lit-labs/context";
import { createMachine, interpret } from "xstate";

export const createService = () => {
  const machine = createMachine({
    id: "hasiApp",

    context: {},

    initial: "green",
    states: {
      green: {
        on: {
          PLOT_PARAMETER_CHANGED: { actions: (c, e) => console.log(e) },
        },
      },
    },
  });
  const service = interpret(machine);
  service.start();
  return service;
};

export interface HasiService {
  service: ReturnType<typeof createService>;
}

export const hasiContext = createContext<HasiService>("hasiService");
