# Log 4

## X-CODE DEFINITIONS

I have done some thinking on how x-codes should be structured to offer a good amount of information density, while being easy to decode. There are some primary properties I think x-codes should have:

1. Declaration of the Encoding Space: x-codes need to declare their encoding space. The fact that they are non-binary makes this necessary. I considered putting the encoding space in only the name-space declaration but that requires encoding consistency across an entire document (potentially through thousands of pages). Sufficient room will be left for such a scenario; but the default will be that all x-codes carry their encoding space.
2. Consecutive x-codes should never share a code-point/letter.
3. X-codes should end with a special terminating sequence.

### ENCODING SPACE DECLARATION OPTIONS
1. Full Ascending(A1): The x-code begins with a sequence of all possible code-points in its encoding space -- from w-white to b-black.
2. Half Ascending(A2): The x-code begins with a sequence of alternating code-points in its encoding space -- from w-white to b-black. In this case, the number of code-points, n must be odd, and thus the code-points in the declaration section are n//2 + 1
3. Full Descending(B1): Similar to (A1) but with code-points in reverse order (b-black to w-white).
4. Half Descending(B2): Similar to (A2) but with code-points in reverse order (b-black to w-white).
5. Ascending Middle Encoding(C1): Special-case declaration for the 4-point space; begins with an ascending sequence of the two middle codepoints in the 4-point space.
6. Descending Middle Encoding(C2): Similar to (C1) but with the code-points in reverse order.

### X-CODE TERMINATION
X-codes may be terminated in one of two ways: 
1. Y-termination: The x-code is terminated with a transition from b-black to w-white.
2. Z-termination: The x-code is terminated with a transition from w-white to b-black.


### GENERAL COMMENTARY
I'm reluctant to standardise anything at this point but here's what I think of some definition styles:
1. A1-Y, A2-Y: I like this style of x-codes because the beginning and end would blend into the page (usually), making the x-code a lot less intrusive. The disadvantage of this is that it's not obvious where the x-code begins and ends.
2. B1-Z, B2-Z: I like these because it's completey obvious where the code begins and ends, requiring no leaps from the user.
3. A1, B1: These ought to make decoding a lot easier, since all points in the encoding space are declared.
4. A2, B2: These minimize the amount of overhead space spent on the declaration section.
5. C1, C2: These are special declarations for the 4-point code space. If the letters/code-points are (A,B,C,D) then B and C are declared while A and D are inferred. The details are still fuzzy, but I think this could work since there are no other declarations with only 2 letters. 

### THAT'S TOO MANY OPTIONS??
Yes, I agree. But in the spirit of experimentation, all possible declaration-termination combinations are under consideration. Ultimately, I think there will need to be some standardization here. This is what I think that could look like:
1. A1-style encodings only for even-numbered code-spaces.
2. B2-style encodings only for odd-numbered code-spaces.
3. Z-style terminations for all code-spaces.
4. C1 only for the 4-point code-space


I might add illustrative pictures later on. However, you can find examples of most of the different possibile x-code types I've discussed [here](../bnw/data/sample_pages), and the code for generating them [here](../bnw/data/x_generator.py)




