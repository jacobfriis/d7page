## List lodgings in two blocks ##


### Requirements ###

PHP 7.

### Installation ###

Normal D7 module installation.

Creates DB table _d7page_lodging_,  
and reads lodgings into that from in-module JSON file.

### Configuration ###

Add two blocks to the overall page template:
- in admin backend, go to Structure > Blocks (/admin/structure/block)
- add _Lodging list_ to the _Content_ region
- add _Lodging entry_ to the _Content_ region

### Use ###

Go to an ordinary page, like /node.

The _Lodging list_ will list all the lodgings.

The _Lodging entry_ remains hidden, unless a lodging is selected:
- click on a _Headline_ in the _Lodging list_
