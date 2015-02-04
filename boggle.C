// 2015 - Lee Chu
//
// This is a simple program that, given a boggle game board and
// a dictionary, finds all the words that can be spelled with the
// game board.
//

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// We only need to worry about letters a-z
//
#define ALPHABET_SIZE 26

// Expected command line arguments:
// ./boggle game_board_file dictionary_file
//
#define ARG_BOARDFILE 1
#define ARG_DICTFILE 2
#define ARG_MAX (ARG_DICTFILE+1)

// Just assume that each line we're reading in from file is no longer than
// this many bytes
//
#define FILE_LINE_SIZE 128

// Max word length
//
#define MAX_WORD_LENGTH 80

// This is used to store our dictionary and provide quick lookup for words.
//
struct Trie
{
  char ch;
  int flags;
  Trie *child[ALPHABET_SIZE];
};

// The trie's flags field stores anything defined here.
//
#define FLAGS_ISWORD 0x1

// This is our main control block for playing Boggle.
//
struct BoggleCB
{
  // Stores the word currently being spelled/worked on
  //
  char search[MAX_WORD_LENGTH];

  // Our dictionary
  //
  Trie *dict;

  // Simply counts the number of times that a letter appears in the
  // game board.  It is used mainly for efficiency when building
  // the dictionary.
  //
  int *histogram;

  // This stores our game board.
  //
  char *board;

  // Used during recursive calls to specify whether or not a letter has already
  // been used to spell the current word.
  //
  bool *used;

  // These are used for sanity checking purposes.  The number of memory allocation
  // calls should be exactly the same as the number of free() calls when this
  // program terminates
  //
  size_t trieAllocCalls;
  size_t trieFreeCalls;

  int boardSize;
  int maxBoardSize;
};

// Given a row and column number, convert it into a flat array
// index
//
inline int getBoardIndex( BoggleCB *bCB, 
                          int row, 
                          int col)
{
  return ( (bCB->boardSize*row) + col);
}

// We only care about the letters a-z .  This converts a letter's ASCII value
// into a number from 0-25 so that we can index into an array.
//
inline int getCharIndex( char c )
{
  char base = 'a';
  return ( tolower(c) - base );
}

// Remove trailing characters such as newline.
//
inline int chop( char *buf )
{
  int stringLength = strlen(buf);

  while ( !isalpha(buf[stringLength-1]) )
  {
    buf[stringLength-1] = '\0';
    stringLength--;
  }

  return stringLength;
}

// Used to allocate memory for a trie node and is used by trieBuild().
//
Trie *trieAllocNode( void )
{
  Trie *newNode = (Trie *)malloc(sizeof(*newNode));

  if ( newNode != NULL )
  {
    memset(newNode, '\0', sizeof(*newNode));
  }

  return newNode;
}

// Attempts to add a dictionary word to the trie.  There are
// some cheap optimizations to filter out words that should
// not be added, such as words that contain letters that
// do not even exist in the game board.
//
bool trieAddWord( BoggleCB *bCB,
                  char *word,
                  bool *wordAdded )
{
  int i = 0;
  int stringLength = strlen(word);
  Trie *curNode = bCB->dict;
  bool bSuccess = true;
  Trie *prevNode = curNode;

  (*wordAdded) = false;

  for ( i = 0; i < stringLength; i++ )
  {
    int ix = getCharIndex(word[i]);

    // A rudimentary optimization.  If the character
    // does not exist in the histogram, exit out
    // and abandon this word.
    //
    // If we really wanted to be efficient, we would count how many 
    // times we've used each letter and confirm that the histogram
    // contains the same count for each letter.
    //
    if ( bCB->histogram[ix] == 0 )
    {
      break;
    }

    // Traverse down to the child node and add a new one if needed.
    //
    if ( curNode->child[ix] == NULL )
    {
      curNode->child[ix] = trieAllocNode();
      if ( curNode->child[ix] == NULL )
      {
        bSuccess = false;
        goto exit;
      }
      bCB->trieAllocCalls++;
    }

    prevNode = curNode->child[ix];
    curNode->child[ix]->ch = word[i];
    curNode = curNode->child[ix];
  }

  // Mark the last node with a flag indicating that this node signifies a 
  // dictionary word.
  //
  if ( prevNode && 
       i == stringLength )
  {
    assert( prevNode != bCB->dict);
    prevNode->flags |= FLAGS_ISWORD;
    (*wordAdded) = true;
  }

exit:
  return bSuccess;
}

// Initialize our Boggle control block
//
void initBoggle( BoggleCB *bCB )
{
  bCB->trieAllocCalls = 
    bCB->trieFreeCalls = 0;

  // Add a root node to the trie
  //
  bCB->dict = trieAllocNode();
  bCB->trieAllocCalls++;
}

// Recursively free the trie that stores our dictionary
//
void trieFree( BoggleCB *bCB,
               Trie **node )
{
  if ( *node != NULL )
  {
    for ( int i = 0; i < ALPHABET_SIZE ; i++ )
    {
      trieFree(bCB, &(*node)->child[i]);
    }

    free (*node );
    *node = NULL;

    bCB->trieFreeCalls++;
  }
}

// Load the dictionary file and store the words in memory.  Some filtering
// takes place on-the-fly so that we skip words that couldn't possibly
// be spelled with the given game board.
//
bool trieBuild( BoggleCB *bCB,
                FILE *fp )
{
  bool bSuccess = true;
  int *histogram = bCB->histogram;
  int stringLength = 0;
  size_t wordCount = 0;
  int maxStringLength = bCB->boardSize*bCB->boardSize;
  char buf[FILE_LINE_SIZE];
  bool wordAdded = false;

  // fgets reads in a line at a time.  Build the trie as
  // we read in the file
  //
  while ( fgets(buf, sizeof(buf), fp) != NULL )
  {
    wordAdded = false;
    stringLength = chop(buf);

    // Skip words that exceed the size of the game board.
    //
    if ( stringLength <= maxStringLength )
    {
      // Add this guy to the trie.  Filtering occurs inside
      // trieAddWord().
      //
      if( !trieAddWord(bCB, buf, &wordAdded) )
      {
        // Hit an error.. bail out
        //
        bSuccess = false;
        goto exit;
      }

      if ( wordAdded ) 
      {
        wordCount++;
      }
    }
  }

  printf("Filtered dictionary down to %lu words\n", wordCount );

exit:
  return bSuccess;
}

// When we're solving the game board, we need a way to keep track of
// what letters have been used and also a way to keep track of what
// word we are currently spelling.  The two functions markUnused() and
// markUsed() help us to do this.
//
inline void markUnused( BoggleCB *bCB, 
                        int row, 
                        int col, 
                        int stringIndex )
{
  int boardIndex = getBoardIndex(bCB, row, col);
  assert(bCB->used[boardIndex] == true);

  bCB->used[boardIndex] = false;

  bCB->search[stringIndex] = '\0';
}

inline void markUsed( BoggleCB *bCB, 
                      int row, 
                      int col, 
                      int stringIndex )
{
  int boardIndex = getBoardIndex(bCB, row, col);
  assert(bCB->used[boardIndex] == false);

  bCB->used[boardIndex] = true;

  bCB->search[stringIndex] = bCB->board[boardIndex];
}

// Returns true if the move specified by boardIndex is a valid move.
// It could be invalid if it brings us off the game board (ie. invalid
// row or column position) or if the move couldn't result in a
// correct word being spelled.
//
inline bool isValid( BoggleCB *bCB, 
                     int boardIndex, 
                     Trie *node )
{
  // The move is valid if the square isn't in use and it is adjacent
  //
  bool bValid = false;

  // Condition 1: Move is within the bounds of the board
  // Condition 2: Letter hasn't already been used to spell
  //              the current word.
  // Condition 3: The string being spelled up to this point
  //              exists in our dictionary.
  //
  if ( boardIndex >=0 && boardIndex <= bCB->maxBoardSize &&
       !bCB->used[boardIndex] &&
       node->child[getCharIndex(bCB->board[boardIndex])  ] != NULL )
  {
    bValid = true;
  }

  return bValid;
}

// Given a row and column number, recursively try all the adjacent tiles.
//
void findSolution( BoggleCB *bCB, 
                   int row, 
                   int col, 
                   Trie *node, 
                   int stringIndex )
{
  // We should always be going down a valid path in the trie.
  //
  assert(node != NULL );

  // We have arrived at a word node and have successfully spelled
  // a word.
  //
  if ( node->flags & FLAGS_ISWORD )
  {
    printf("Found word %s (%d,%d)\n", bCB->search, row, col);
  }

  // Keep going, in case the word is a prefix for another word.
  //
  for ( int rowDiff = -1; rowDiff < 2; rowDiff++ )
  {
    int newRow = row+rowDiff;

    if ( newRow >= 0 && newRow < bCB->boardSize )
    {
      for ( int colDiff = -1; 
                colDiff < 2; 
                colDiff++ )
      {
        int newCol = col+colDiff;
        int move = getBoardIndex(bCB, newRow, newCol);

        if ( newCol >= 0 &&
             newCol < bCB->boardSize &&
             isValid(bCB, move, node) )
        {
          int newIx = getCharIndex( bCB->board[move] );

          markUsed(bCB, newRow, newCol, stringIndex);

          findSolution(bCB,
                       newRow,
                       newCol,
                       node->child[ newIx ],
                       stringIndex+1);

          // Backtrack
          //
          markUnused(bCB, newRow, newCol, stringIndex);
        }
      }
    }
  }
}

// This is the root function that calls findSolution() for each game 
// tile.  findSolution() is a recursive function that will visit the
// adjacent tiles.
//
void playBoggle( BoggleCB *bCB )
{
  int charIndex = 0;

  assert(sizeof(bCB->search) >= (bCB->boardSize*bCB->boardSize) );

  // Reset the word we're trying to spell.
  //
  memset(bCB->search, '\0', sizeof(bCB->search) );

  for (int i = 0; i < bCB->boardSize; i++ )
  {
    for ( int j = 0; j < bCB->boardSize; j++ )
    {
      markUsed(bCB, i, j, 0);
      charIndex = getCharIndex( bCB->board[getBoardIndex(bCB,i,j)] ) ;

      findSolution(bCB, i, j, bCB->dict->child[charIndex] , 1 );

      // Backtrack.
      //
      markUnused(bCB, i, j, 0);
    }
  }
}

int main( int argc, char *argv[] )
{
  int rc = 0;
  FILE *fp = NULL;
  char buf[FILE_LINE_SIZE];
  size_t boardSize = 0;
  char *board = NULL;
  size_t allocBytes = 0;
  int row = 0, col = 0;
  int i = 0;
  int *histogram = NULL;
  int stringLength = 0;
  BoggleCB bCB;

  // Initialize to all zeroes
  //
  memset( &bCB, '\0', sizeof(bCB) );

  if ( argc > ARG_MAX )
  {
    printf("Invalid number of args (%d).  Specify the boardFile and the dictionaryFile.\n", argc );
    goto exit;
  }

  initBoggle(&bCB);

  // Read in the input file
  //
  fp = fopen(argv[ARG_BOARDFILE], "r");
  if ( !fp )
  {
    printf("Error opening board file \"%s\"\n", argv[ARG_BOARDFILE]);
    goto exit;
  }

  // fgets reads in a line at a time.
  //
  while ( fgets(buf, sizeof(buf), fp) != NULL )
  {
    col = 0;

    // ABCD\n
    // string length is 5
    //
    stringLength = chop(buf);

    // Board needs initialization
    //
    if ( board == NULL )
    {
      boardSize = stringLength;
      bCB.boardSize = boardSize;

      printf("Allocating enough memory for a %lu x %lu board\n", boardSize, boardSize);
      allocBytes = sizeof(char) * boardSize * boardSize;
      board = (char *)malloc( allocBytes );
      if ( !board )
      {
        printf("Error allocating board memory (%lu bytes)\n", allocBytes );
        goto exit;
      }
      memset(board, '\0', allocBytes );
    }

    // Board layout positions
    //  0  1  2  3
    //  4  5  6  7
    //  8  9 10 11
    // 12 13 14 15
    //
    for ( i = 0; i < boardSize; i++ )
    {
      board[ boardSize*row + i ] = tolower(buf[i]);
    }

    row++;
  }

  // Done with the file
  //
  fclose(fp);
  fp = NULL;

  // Print out the board
  //
  for ( row = 0; row  <  boardSize; row++ )
  {
    for ( col = 0; col < boardSize; col++ )
    {
      printf("%2c", board[row*boardSize + col] );
    }
    printf("\n");
  }

  // Now we need to build a trie that represents the dictionary words.
  // There are a few things we can do to prune the dictionary.
  // 1) Discard words of length longer than boardSize*boardSize
  // 2) We can keep a histogram of character counts, based on
  //    the game board.  We use the game board because it is likely
  //    going to be smaller than the dictionary itself.  If a dictionary
  //    word contains a character not in the histogram, we can discard
  //    the word.
  //

  // Build the histogram.  We do it here for clarity.  We could do
  // it at the same time that we scan the file, if we really wanted
  // to be efficient.  Since we are only using characters from a-z,
  // we know exactly how much memory we need.  array index 0 will 
  // be character 'a' (decimal 97, or x61 ).  array index 1 will
  // be 'b', 2 will be 'c', and so on.
  //
  // Note, for my own information: 'A' is decimal 65, or x41
  //
  allocBytes = ALPHABET_SIZE*sizeof(int);
  histogram = (int *)malloc(allocBytes);
  if ( !histogram )
  {
    printf("Could not allocate memory for histogram (%lu bytes)\n", allocBytes );
    goto exit;
  }
  memset(histogram, '\0', allocBytes);

  for ( row = 0; row < boardSize; row++ )
  {
    for (col = 0; col < boardSize; col++ )
    {
      int arrayIndex = getCharIndex( board[getBoardIndex(&bCB, row, col)] );
      assert(arrayIndex < ALPHABET_SIZE);
      histogram[arrayIndex]++;
    }
  }

  // Anchor the blocks of memory to our boggle control block.
  //
  bCB.histogram = histogram;
  bCB.board     = board;
  bCB.maxBoardSize = bCB.boardSize*bCB.boardSize;

  allocBytes = sizeof(*bCB.used) * boardSize*boardSize;
  bCB.used = (bool *)malloc(allocBytes);
  if ( !bCB.used )
  {
    printf("Failed to allocate memory for bool array\n");
    goto exit;
  }
  memset( bCB.used, '\0', allocBytes );

  // Read in the dictionary words
  //
  fp = fopen(argv[ARG_DICTFILE], "r");
  if ( !fp )
  {
    printf("Error opening dictionary file \"%s\"\n", argv[ARG_DICTFILE]);
    goto exit;
  }

  // Build our trie, which allows us to quickly search for valid words
  //
  if ( !trieBuild(&bCB, fp) )
  {
    printf("Error building trie\n");
    goto exit;
  }

  // Finally, we can solve the game board.
  //
  playBoggle(&bCB);

exit:
  // Release resources
  //
  trieFree(&bCB, &bCB.dict);
  printf("num alloc calls = %lu, num free calls = %lu\n",
          bCB.trieAllocCalls,
          bCB.trieFreeCalls);
  assert( bCB.dict == NULL );
  assert( bCB.trieAllocCalls == bCB.trieFreeCalls );

  if ( fp )
  {
    fclose(fp);
    fp = NULL;
  }

  if ( board )
  {
    free(board);
    board = NULL;
    bCB.board = NULL;
  }

  if ( histogram )
  {
    free( histogram );
    histogram = NULL;
    bCB.histogram = NULL;
  }

  if ( bCB.used )
  {
    free(bCB.used);
    bCB.used = NULL;
  }

  return rc;
}
