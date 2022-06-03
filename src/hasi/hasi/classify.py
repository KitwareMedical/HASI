#!/usr/bin/env python3

# Copyright NumFOCUS
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#        https://www.apache.org/licenses/LICENSE-2.0.txt
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Purpose: Python functions for shape classification and distance
#           analysis with DWD classifier
import itk
import numpy as np
from dwd.dwd import DWD

import seaborn as sns
import matplotlib.pyplot as plt

# Generates an nxd feature matrix with
# - n = number of meshes (samples)
# - d = number of features, equal to
#       number of mesh points * (1 / step) * mesh dimension
def make_point_features(meshes:list,
                        step:int=1) -> np.ndarray:
    assert(step >= 1)

    features = None
    for mesh in meshes:
        points = [mesh.GetPoint(idx)
                  for idx in range(mesh.GetNumberOfPoints())
                  if idx % step == 0]
        points_array = np.expand_dims(np.asarray(points).flatten(),0)

        if features is None:
            features = points_array
        else:
            features = np.append(features, points_array, axis=0)
    return features
    
def get_distances(classifier:DWD, features:np.ndarray) -> np.ndarray:
    direction = classifier.coef_.reshape(-1)
    intercept = float(classifier.intercept_)
    distance = features.dot(direction) + intercept

    return distance

def densityplot(X, y, ax=None):
    ax = ax or plt.gca()

    xlim = X.min() * 1.05, X.max() * 1.05
    ax.set_xlim(*xlim)

    ax.axvline(x=0, color='green', linestyle='dashed')

    order = sorted(np.unique(y))
    sns.kdeplot(ax=ax, x=X,
        color='black',
        common_norm=True,
        common_grid=True,
        bw_adjust=1.0,)

    sns.kdeplot(ax=ax, x=X,
        hue=y, hue_order=order,
        common_norm=True,
        common_grid=True,
        bw_adjust=1.0)

    sns.rugplot(ax=ax, x=X,
        hue=y, hue_order=order,)

    sns.despine()

# Ridge Plots
def density_by(X, category, categories=None):
    # adapted from https://seaborn.pydata.org/examples/kde_ridgeplot.html

    plt.close()

    variable = 'Distance'
    if not categories:
        categories = X[category].unique().tolist()

    title = '{} density by {}'.format(variable, category)

    g = sns.FacetGrid(X,
        row=category,
        hue=category,
        row_order=categories,
        hue_order=categories,
        aspect=8,
        height=1)

    g.map(sns.kdeplot, variable, fill=True)
    g.map(plt.axhline, y=0, lw=2)
    g.map(plt.axvline, x=0, lw=2, color='k')

    def label(x, color, label):
        ax = plt.gca()
        ax.text(0, .3, label,
            ha='left', va='center',
            transform=ax.transAxes)

    g.map(label, category)

    g.set_titles("")
    g.set(yticks=[])
    g.despine()
