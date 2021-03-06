#! /usr/bin/perl -w

open(FOO, "main-a.c") || die "Did you delete main-a.c?";
$lines = 0;
$lines++ while defined($_ = <FOO>);
close FOO;

@tests = (
# Execute
	  [ 'echo Hooray',
	    'Hooray' ],
	  [ 'echo Semi ;',
	    'Semi' ],
	  [ 'echo Line & sleep 1',
	    'Line' ],
	  [ 'echo "" """" EOL',
	    'EOL' ],
	  [ 'echo He"llo wo"rld',
	    'Hello world' ],
	  [ 'echo Hello   World',
	    'Hello World' ],
# Use a vertical tab to avoid tab completion
	  [ 'echo Hello World',
	    'Hello World' ],
	  [ 'echo Pipe | cat',
	    'Pipe' ],

# Conditional
	  [ 'true && echo True',
	    'True' ],
	  [ 'echo True || echo False',
	    'True' ],
	  [ 'grep -cv NotThere main-a.c && echo False',
	    "$lines False" ],
	  [ 'false || echo True',
	    'True' ],
	  [ 'true && false || true && echo Good',
	    'Good' ],
	  [ 'echo True && false || false && echo Bad',
	    'True' ],
# Test that conditional status is inherited through parentheses
	  [ '( ( false || echo False && false ) || ( echo Inherited ) ) && echo Nested',
	    'False Inherited Nested' ],

# Pipe
	  [ 'echo Good | grep G',
	    'Good' ],
	  [ 'echo Bad | grep -c G',
	    '0' ],
	  [ 'echo Line | cat | wc -l',
	    '1' ],
	  [ 'echo GoHangASalamiImALasagnaHog | rev | tee temp.out | rev | rev',
	    'goHangasaLAmIimalaSAgnaHoG' ],
	  [ 'rev temp.out | rev',
	    'goHangasaLAmIimalaSAgnaHoG' ],
	  [ 'cat temp.out | tr [A-Z] [a-z] | md5sum',
	    '8e21d03f7955611616bcd2337fe9eac1 -' ],
	  [ 'rev temp.out | md5sum | tr [a-z] [A-Z]',
	    '502B109B37EC769342948826736FA063 -' ],

# Sequential
	  [ 'echo Hello ; echo There',
	    'Hello There' ],
	  [ 'echo Hello ;   echo There ; echo Who ; echo Are ; echo You ; echo ?',
	    'Hello There Who Are You ?' ],
	  [ 'rm -f temp.out ; echo Removed',
	    'Removed' ],
	  [ 'sleep 2 ; ps -C sleep ; echo Done',
	    'PID TTY TIME CMD Done' ],

# Parentheses
	  [ '( echo Line )',
	    'Line' ],
	  [ '( echo Hello ; echo World ) | wc -l',
	    '2' ],
	  [ '( echo Line ; echo Line ) | uniq | wc -l',
	    '1' ],
	  [ '( echo Hello -" ) " World " ( "- ; echo Next ) | cat',
	    'Hello - ) World ( - Next' ],
	  [ '( true ) && echo True',
	    'True' ],
	  [ '( false || true ) && echo True',
	    'True' ],
	  [ '( sleep 1 ; echo Sleep ) & echo Wake ; sleep 2',
	    'Wake Sleep' ],

# BuildMultiLine (setup for Multiple-lines case below)
	  [ 'echo echo Line 1 > temp.out ; echo echo Line 2 | cat temp.out - > temp2.out ; mv -f temp2.out temp.out ; echo echo Line 3 | cat temp.out - > temp2.out ; mv -f temp2.out temp.out ; echo Build',
	    'Build' ],

# Multiple-lines
	  [ './ospsh -q < temp.out',
	    'Line 1 Line 2 Line 3' ],

# Redirection
	  [ 'echo Start ; echo File > temp.out',
	    'Start' ],
	  [ 'cat < temp.out ; echo Done',
	    'File Done' ],
	  [ 'rm file_that_is_not_there 2> temp.out ; wc -l temp.out ; rm -f temp.out',
	    '1 temp.out' ],
# Test simultaneous input and output redirection
	  [ '( echo Hello ; echo Bye ) > temp.out ; cat < temp.out > temp2.out ; cat temp.out temp2.out',
	    'Hello Bye Hello Bye' ],
	  [ 'sort < temp.out | ( head -n 2 ; echo First && echo Good )',
	    'Bye Hello First Good' ],
	  [ 'sort < temp.out > temp2.out ; tail -n 2 temp2.out ; rm -f temp.out temp2.out',
	    'Bye Hello' ],

# Background
# Test that sleep is backgrounded and later commands are run
	  [ 'sleep 2 & ps -C sleep | wc -l',
	    '2' ],
# Test parentheses
	  [ '( sleep 2 ) & ps -C sleep | wc -l',
	    '2' ],
# Test multiple background commands
	  [ '( echo Hello ; sleep 2 ) & sleep 1 ; ps -C sleep | wc -l',
	    'Hello 2' ],
	  [ '( echo Hello ; sleep 2 & echo Bye ) ; ps -C sleep | wc -l',
	    'Hello Bye 2' ],

# cd
	  [ 'cd / ; pwd',
	    '/' ],
	  [ 'cd / ; cd /tmp ; pwd',
	    '/tmp' ],

# cd without redirecting stdout
	  [ 'cd / ; cd /doesnotexist 2> /dev/null ; pwd',
	    '/' ],
	  [ 'cd / ; ( cd /tmp ) ; pwd',
	    '/' ],

# Fancy conditionals
	  [ 'cd / && pwd',
	    '/' ],
	  [ 'echo go ; cd /doesnotexist 2> /dev/null > /dev/null && pwd',
	    'go' ],
	  [ 'cd /doesnotexist 2> /dev/null > /dev/null || echo does not exist',
	    'does not exist' ],
	  [ 'cd /tmp && cd / && pwd',
	    '/' ],
	  [ 'cd / ; ( cd /doesnotexist1 2> /dev/null > /dev/null || cd / || cd /doesnotexist2 2> /dev/null > /dev/null ) && pwd',
	    '/' ],
	  [ 'cd / ; cd /doesnotexist 2> /dev/null > /dev/null ; pwd',
	    '/' ],

# BuildMultiLine (setup for Exit case below)
	  [ 'echo echo Line 1 > temp.out ; echo exit | cat temp.out - > temp2.out ; mv -f temp2.out temp.out ; echo echo Line 3 | cat temp.out - > temp2.out ; mv -f temp2.out temp.out ; cat temp.out',
	    'echo Line 1 exit echo Line 3' ],

# Exit
	  [ './ospsh -q < temp.out',
	    'Line 1' ],
	  [ 'echo Before ; exit ; echo After',
	    'Before' ],
	  [ '( exit ) ; echo After',
	    'After' ],

# BuildMultiLine
	  [ 'rm -f temp.out ; echo echo Line 1 "&" sleep 1 > temp1.out ; echo echo Line 2 | cat temp1.out - > tempt.out ; mv -f tempt.out temp1.out ; cat temp1.out',
	    'echo Line 1 & sleep 1 echo Line 2' ],
	  [ 'echo "sleep 2 & ps -C sleep | wc -l" > temp2.out ; echo "sleep 3" | cat temp2.out - > tempt.out ; mv -f tempt.out temp2.out ; cat temp2.out',
	    'sleep 2 & ps -C sleep | wc -l sleep 3' ],
	  [ 'echo "ps -C sleep | wc -l" | cat temp2.out - > tempt.out ; mv -f tempt.out temp2.out ; cat temp2.out',
	    'sleep 2 & ps -C sleep | wc -l sleep 3 ps -C sleep | wc -l' ],

# Zombie
# Method 1
	  [ 'cat temp1.out | ./ospsh -q & sleep 2 ; ps -C echo | grep "<defunct>$"',
	    'Line 1 Line 2' ],
# Method 2
	  [ './ospsh -q < temp2.out',
	    '2 1' ],

# CleanupMultiLine
	  [ 'rm -f temp1.out temp2.out ; echo clean',
	    'clean' ],
	  );

my($ntest) = 0;

my($sh) = "./ospsh";
my($tempfile) = "lab1btest.txt";
my($ntestfailed) = 0;

if (!-x $sh) {
    print STDERR "$sh does not exist, so I can't run any tests!\n(Try running \"gmake\" to create $sh.)\n";
    exit(1);
}

foreach $test (@tests) {
    $ntest++;
    print STDOUT "Starting test $ntest\n";
    my($in, $want) = @$test;
    open(F, ">$tempfile") || die;
    print F $in, "\n";
    close(F);
    $result = `$sh -q < $tempfile 2>/dev/null`;
    $result =~ s%^cs111_fall07\$ %%;
    $result =~ s%cs111_fall07\$ $%%;
    $result =~ s|\[\d+\]||g;
    $result =~ s|^\s+||g;
    $result =~ s|\s+| |g;
    $result =~ s|\s+$||;
    next if $result eq $want;
    next if $want eq 'Syntax error [NULL]' && $result eq '[NULL]';
    next if $result eq $want;
    print STDERR "Test $ntest FAILED!\n  input was \"$in\"\n  expected output like \"$want\"\n  got \"$result\"\n";
    $ntestfailed += 1;
}

unlink($tempfile);
my($ntestpassed) = $ntest - $ntestfailed;
print "$ntestpassed of $ntest tests passed\n";
exit(0);
