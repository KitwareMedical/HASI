import { createContext } from "@lit-labs/context";
import { createMachine, interpret, assign } from "xstate";
import { Field, fields } from "../scan.types";

export type PlotParameter = "leftBiomarker" | "bottomBiomarker";

export const createService = () => {
  const machine = createMachine(
    {
      tsTypes: {} as import("./hasi.machine.typegen").Typegen0,
      schema: {
        context: {} as {
          plotParameters: { leftBiomarker: Field; bottomBiomarker: Field };
        },
        events: {} as {
          type: "PLOT_PARAMETER_CHANGED";
          parameter: PlotParameter;
          value: Field;
        },
      },

      id: "hasiApp",

      context: {
        plotParameters: {
          leftBiomarker: fields[0],
          bottomBiomarker: fields[1],
        },
      },

      initial: "running",
      states: {
        running: {
          on: {
            PLOT_PARAMETER_CHANGED: { actions: "assignParameter" },
          },
        },
      },
    },
    {
      actions: {
        assignParameter: assign((c, e) => {
          return {
            plotParameters: {
              ...c.plotParameters,
              [e.parameter]: e.value,
            },
          };
        }),
      },
    }
  );

  const service = interpret(machine);
  service.start();
  return service;
};

export interface HasiService {
  service: ReturnType<typeof createService>;
}

export const hasiContext = createContext<HasiService>("hasiService");
