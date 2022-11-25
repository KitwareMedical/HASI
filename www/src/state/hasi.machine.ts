import { createContext } from "@lit-labs/context";
import { createMachine, interpret, assign } from "xstate";

import { Field, fields, ScanId } from "../scan.types.js";

export type PlotParameter = "leftBiomarker" | "bottomBiomarker";
export type ScanClicked = {
  type: "SCAN_CLICKED";
  id: ScanId;
};

export const createService = () => {
  const machine = createMachine(
    {
      tsTypes: {} as import("./hasi.machine.typegen").Typegen0,
      schema: {
        context: {} as {
          selectedScans: Set<ScanId>;
          plotParameters: { leftBiomarker: Field; bottomBiomarker: Field };
        },
        events: {} as
          | {
              type: "PLOT_PARAMETER_CHANGED";
              parameter: PlotParameter;
              value: Field;
            }
          | ScanClicked,
      },
      predictableActionArguments: true,

      id: "hasiApp",

      context: {
        selectedScans: new Set<ScanId>(),
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
            SCAN_CLICKED: { actions: "toggleScanSelected" },
          },
        },
      },
    },
    {
      actions: {
        toggleScanSelected: assign(({ selectedScans }, { id }) => {
          if (!selectedScans.delete(id)) selectedScans.add(id);
          return { selectedScans };
        }),

        assignParameter: assign(({ plotParameters }, { parameter, value }) => {
          return {
            plotParameters: {
              ...plotParameters,
              [parameter]: value,
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
