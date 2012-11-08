Note Select is a simple program in vanilla C and GTK so it will compile straight off.  The source is all in one file.

The metaphor is a disorganized pile of scraps of paper with the ability to instantly retrieve the particular note that you seek.

The notes are placed anywhere you like on an infinite sized layout container.  Each note is a GTK Text View so all editing functions, copy, paste, etc. are available.  Clicking a note highlights it.

- To move notes around the layout area use ctrl-click and drag.

In the menu bar are several functions:

- Note: creates a new note.

- Delete: deletes the currently selected (highlighted) note.

- Resize: click on a note to select it, then hit the resize button and use the arrow keys to manipulate the size of the note.

- The Repack menu button needs to be implemented.  Its function is to reorganize / repack the notes on the layout area in some visually pleasing way.  There might want to be a submenu here to allow various repacking schemes such as straight down the page, tiled with no overlap, tiled or cascaded with overlap, and sorting of notes alphabetically.  The repack function needs to be invoked after each keystroke during searches.

Under the file menu are several functions.  You can save the stack, save the stack as a new stack, create a new stack, save selected notes (those that meet search criteria) as a new stack, export a stack in a simple text format that can be brought into a word processor or whatever, and you can import a stack file, adding its notes to the stack you are currently editing.

- The most important thing about Note Select is what I call the Find function. It opens a non-modal dialog where you can type search text. With each keystroke in the search box, a search is performed and notes that do not meet the criteria are hidden. There is a function under the File menu to save just the displayed notes as a new stack.

To see this in operation, open the sample stack file, test2.stk stack, hit Find.  Type "3", and you'll see how it's supposed to work. Only the notes containing the digit "3" remain visible. Hit backspace, and the rest reappear.

One way to use the program is for phone numbers. What I do is create 26 notes, one for each letter of the alphabet. The top line of each note is AAAA, BBBB, CCCC, etc. Then name, addr, phones below that. If I want the M's, I hit Find and type MMM and see the note with all the M's. If I remember a fragment of a phone number like 457, I search for 457 and there it is. Very simple but effective.

I use it to organize fragments of information for news articles, characters for novels, and so on. I used it to write this message.

- The stack file format is very simple.  It's all text and can be inspected with a hex dumper or some text editors.  As implemented it will handle full UTF-8 coding. 

- Note Select is not meant for huge amounts of data. Searching is done brute-force. I don't think I've ever had a stack with more than 200 notes in it and brute force works fine.

I have no desire to own the code or ideas. I'd be happy if it was of use to somebody else. I've been frustrated for years that an InfoSelect knockoff didn't exist because the basic concept is so simple.