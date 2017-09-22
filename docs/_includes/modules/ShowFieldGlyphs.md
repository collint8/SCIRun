---
title: ShowFieldGlyphs
category: moduledocs
module:
  category: Visualization
  package: SCIRun
tags: module

---

# {{ page.title }}

## Category

**{{ page.module.category }}**

## Description

### Summary

This module visualizes the data that makes up a Field. When possible and selected, the glyph takes its color from the data values that permeate the field.

**Detailed Description**

The field in the first input port holds the data that is to be visualized. By default is will be displayed using the the default color, which is editable from the GUI If there is a color map attached to the second input port, then the data can be used as an index into the color map, and the glyph is rendered with appropriate colors. In addition, the data itself can be converted into a color. **Scalar** data creates a gray scale mapping, **vector** data (normalized) creates RGB colors, and the **principle eigenvector**c (normalized) of tensor data also creates RGB colors. Vector magnitude, or the primary eigenvalue for tensors, is used as the index into the color map. The absolute value of vector x, y, z components are used to generate RGB color components:

```
       (R = |x|, G = |y|, B = |z|)
```

It is possible to use a secondary or tertiary data to control the glyph parameters. For instance, when rendering vectors as a cone the orientation and length of the cone are determined by the primary field whereas the secondary field can be used to determine the radius and/or color of the cone.

{% capture url %}{% include url.md %}{% endcapture %}
{{ url }}
