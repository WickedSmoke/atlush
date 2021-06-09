Atlush
======

Atlush is an image atlas editor which tracks rectangular regions on an
image or set of images.

![Screenshot](http://outguard.sourceforge.net/images/atlush-0.1.png)

Here are the basic functions:

  * Define named rectangles on images.
  * Automatically pack images.
  * Extract regions from images.
  * Merge images together.
  * Search for region by name.
  * Define import & export commands for different projects.


Managing Images & Regions
-------------------------

The Add Image action (**Ins** key) loads an image into the workspace.
Import Directory will import all PNG & JPEG files from a directory into
the workspace.

Regions can be added to a selected image with the Add Region action.

Images & regions can be removed by selecting them and using the Remove Item
action (**Del** key).

Images & regions can be moved by either dragging them with the mouse or by
using the position value widgets on the toolbar.  When an image is moved the
associated regions move with it.

Regions can be resized by either dragging the corners with the mouse or by
using the size value widgets on the toolbar.

### Export Image

Create a new image file containing the pixel data of all images in the
workspace.


Packing Tools
-------------

### Pack Images

Pack Images moves the selected images together into the top-left corner
of the canvas.  Pixel padding between images is controlled by the **Pad**
toolbar widget.  The **Sort** toolbar option can be checked to sort the
images by size before packing.

### Merge Images

Merge all images in the workspace into a single image.  All regions are
transferred to the new image and the original images are then removed.

### Extract Regions

Extract Regions copies the pixel data under the selected regions into a new
image.  The source images are removed from the workspace.

### Regions to Images

Create new PNG images from selected regions and then remove those regions.
The image file names will be copied from the region name.

### Crop Images

Remove any fully transparent outside areas from selected images.


Command Line Arguments
----------------------

A single project file or image directory can be provided on the command line.

If the argument is a directory path then all .png, .jpeg, & .jpg files will
be imported.


How to Compile
--------------

Qt 5 is required.  Project files are provided for QMake & copr.

To build with QMake:

    qmake-qt5; make

To build with copr:

    copr
