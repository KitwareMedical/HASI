import { createContext } from '@lit-labs/context';
import {
  createMachine,
  interpret,
  assign,
  ContextFrom,
  StateFrom,
} from 'xstate';

import { Field, fields, ScanId, FEATURE_KEYS, Feature } from '../scan.types.js';

export type PlotParameter = 'leftBiomarker' | 'bottomBiomarker';
export type ScanClicked = {
  type: 'SCAN_CLICKED';
  id: ScanId;
};

export function decodeFromBinary(str: string): string {
  return decodeURIComponent(
    Array.prototype.map
      .call(atob(str), function (c) {
        return '%' + ('00' + c.charCodeAt(0).toString(16)).slice(-2);
      })
      .join('')
  );
}
export function encodeToBinary(str: string): string {
  return btoa(
    encodeURIComponent(str).replace(/%([0-9A-F]{2})/g, function (_, p1) {
      return String.fromCharCode(parseInt(p1, 16));
    })
  );
}

const machine = createMachine(
  {
    id: 'hasiApp',
    tsTypes: {} as import('./hasi.machine.typegen').Typegen0,
    schema: {
      context: {} as {
        selectedScans: Set<ScanId>;
        plotParameters: { leftBiomarker: Field; bottomBiomarker: Field };
        features: Array<Feature>;
      },
      events: {} as
        | {
            type: 'PLOT_PARAMETER_CHANGED';
            parameter: PlotParameter;
            value: Field;
          }
        | ScanClicked
        | {
            type: 'FEATURE_SELECT';
            featureIndex: number;
            feature: Feature;
          }
        | {
            type: 'FEATURE_ADD';
          }
        | {
            type: 'FEATURE_REMOVE';
            featureIndex: number;
          },
    },
    predictableActionArguments: true,

    context: {
      selectedScans: new Set<ScanId>(),
      features: [FEATURE_KEYS[0]], // selected features to view
      plotParameters: {
        leftBiomarker: fields[0],
        bottomBiomarker: fields[1],
      },
    },

    initial: 'running',
    states: {
      running: {
        on: {
          PLOT_PARAMETER_CHANGED: { actions: 'assignParameter' },
          SCAN_CLICKED: { actions: 'toggleScanSelected' },
          FEATURE_SELECT: { actions: 'assignFeature' },
          FEATURE_ADD: { actions: 'addFeature' },
          FEATURE_REMOVE: { actions: 'removeFeature' },
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

      assignFeature: assign(({ features }, { featureIndex, feature }) => {
        if (!FEATURE_KEYS.includes(feature)) return { features };
        return {
          features: Object.assign([], features, { [featureIndex]: feature }),
        };
      }),

      addFeature: assign({
        features: ({ features }) => [...features, FEATURE_KEYS[0]],
      }),

      removeFeature: assign({
        features: ({ features }, { featureIndex }) => [
          ...features.slice(0, featureIndex),
          ...features.slice(featureIndex + 1),
        ],
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
export type HasiMachine = typeof machine;

type HasiService = ReturnType<typeof createService>;

function contextToJson(c: ContextFrom<typeof machine>) {
  return JSON.stringify(c, (_, value) => {
    return value instanceof Set ? Array.from(value as Set<ScanId>) : value;
  });
}

function jsonToContext(json: any): ContextFrom<typeof machine> {
  json.selectedScans = new Set(json.selectedScans);
  return json;
}

const STATE_KEY = 'state';

export function saveState(state: StateFrom<HasiMachine>) {
  const json = contextToJson(state.context);

  window.history.replaceState(
    null,
    '',
    `?${STATE_KEY}=${encodeToBinary(json)}`
  );
}

function getSavedState() {
  const stateFromURL = new URL(document.location.href).searchParams.get(
    STATE_KEY
  );
  if (stateFromURL) {
    const json = JSON.parse(decodeFromBinary(stateFromURL));
    return jsonToContext(json);
  }
  return undefined;
}

export const createService = () => {
  const context = getSavedState();
  const m = context ? machine.withContext(context) : machine;

  const service = interpret(m).start();
  service.subscribe((state) => state.changed && saveState(state));
  return service;
};

export interface HasiContext {
  service: HasiService;
}

export const hasiContext = createContext<HasiContext>('hasiService');
