# LineOperations #


## About ##

*Line Operations* is a plugin for *Geany* that gives a convenient option to apply some useful line operations on an open file.

### Features ###

* [Remove Duplicate Lines, sorted](#remove-duplicate-lines)
* [Remove Duplicate Lines, ordered](#remove-duplicate-lines)
* [Remove Unique Lines](#remove-unique-lines)
* [Remove Empty Lines](#remove-empty-lines)
* [Remove Whitespace Lines](#remove-whitespace-lines)
* [Sort Lines Ascending](#sort-lines)
* [Sort Lines Descending](#sort-lines)

## Usage ##

After the plugins has been installed successfully, load the plugin via
Geany's plugin manager and a new menu item in the Tools menu will appear. Click on each menu item to apply operation on whole file. See descriptions below to see operations for each menu item. 

### Notes ###

1. Line Operations will **not** make changes to a file until you save the file.

2. New line characters are considered to be part of the line. If the last line does not contain new line characters it will be considered different from a line with new line character(s).

		line 1

	is not the same as:

		line 1\r\n


### Operation Details ###

#### Remove Duplicate Lines ####

The first occurrence of each duplicate line will remain in the file.**Sorted** option will sort the file and remove duplicate lines **[fast]**. **Ordered** option will keep the same order of lines **[slow]**.

**Example:** Suppose a file has the following lines. (#comments added for clarity)

	Line 2
	Line 1
	Line 2    #removed
	Line 3
	Line 1    #removed
	Line 2    #removed

The **Remove Duplicate Lines, sorted** will change the file into this:

	Line 1
	Line 2
	Line 3

The **Remove Duplicate Lines, ordered** will change the file into this:

	Line 2
	Line 1
	Line 3



#### Remove Unique Lines ####

Removes all lines that appear only once.

**Example:** Suppose a file has the following lines. (#comments added for clarity)

	Line 2
	Line 1
	Line 2
	Line 3    #removed
	Line 1
	Line 2

The 'Remove Unique Lines' will change the file into this:

	Line 2
	Line 1
	Line 2
	Line 1
	Line 2

#### Remove Empty Lines ####

Removes all lines that end with a newline character(s) AND do not have any other characters.

**Example:** Suppose a file has the following lines. (#comments, and \n newline characters added for clarity)

	Line 2\n
	Line 1\n
	\n          #removed
	     \n     #NOT removed (contains spaces)
	    Line 1\n
	Line 2\n

The 'Remove Unique Lines' will change the file into this:

	Line 2\n
	Line 1\n
	     \n
	    Line 1\n
	Line 2\n


#### Remove Whitespace Lines ####

Removes all lines that have only white space characters.

A **white space character** is one of:

1. ```' '``` : space
2. ```'\t'```: horizontal tab
3. ```'\f'```: form feed
4. ```'\v'```: vertical tab
5. ```'\r'```: cartridge return
6. ```'\n'```: newline character

**Example:** Suppose a file has the following lines. (#comments, and \n newline characters added for clarity)

	Line 2\n
	Line 1\n
	\n           #removed
	     \n      #removed (contains only whitespace chars)
	 \t \n       #removed (contains only whitespace chars)
	    Line 1\n #NOT removed (contains non whitespace chars)
	Line 2\n

The 'Remove Unique Lines' will change the file into this:

	Line 2\n
	Line 1\n
	    Line 1\n
	Line 2\n

#### Sort Lines ####

Sorts lines ascending or descending based on ASCII values (lexicographic sort)


**Example:** Suppose a file has the following lines.

	line 1
	line 2
	line
	line 3
	line

The **Sort Lines Ascending** will change the file into this:

	line
	line
	line 1
	line 2
	line 3


The **Sort Lines Descending** will change the file into this:

	line 3
	line 2
	line 1
	line
	line


## License ##

The Line Operations plugin is distributed under the terms of the
GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.
A copy of this license can be found in the file COPYING included with the
source code of this program.

## Ideas, questions, patches and bug reports ##

Please direct all questions, bug reports and patches to the plugin author using the
email address listed below or to the *Geany* mailing list to get some help from other
*Geany* users.


or report them at https://github.com/geany/geany-plugins/issues.


