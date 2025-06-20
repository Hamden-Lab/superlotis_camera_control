#!/usr/bin/perl -w
#
# $Id: sicam_client.pl,v 1.31 2005/11/15 01:31:24 slotis Exp $
#
use Expect;
use Astro::FITS::CFITSIO qw( :longnames );
# use Astro::FITS::CFITSIO;
use Net::Telnet;
use IO::File;
use File::Basename;
use strict;
use warnings;
use POSIX qw(strftime);

# GGW - 10/5/2006
use RRDs;

# JACOB: The camera is NOT powered on via this script, but rather by a cronjob on the camera computer
# It has been that way since 2010
# GGW - 12/15/2010
# I commented out the command to turn off the camera power.  This shouldn't
# be done here. The power is controlled externally.  This line was hanging
# for some reason causing the power to remain off.

#JACOB: we begin
# This is the path and name of the program to control the SI camera from a prompt.
# It will be run using the Perl Expect module.

# This is the sicam shell location on eddy.kpno.noao.edu.
# The sicam shell must initialize or else you get a segmentation fault.
#JACOB: start the camera software. This software initializes the camera, set exposure times, 
# exposes and writes a fits file
# ....first....start the software...
my $sicam_shell = "/home/slotis/sicam/cli/sicam";

#JACOB: The scheduler socket server will send commands to this client...which sends commands to the camera software
# The scheduler socket server.
my $slotis_scheduler_host = "140.252.86.96";
my $slotis_scheduler_port = 5134;

#this sends the status to this client, but also sends camera status info to the status_server
# The status socket server.
my $slotis_status_server_host = "140.252.86.96";
my $slotis_status_server_port = 5135;

# XML Log file details.
my $xml_log_directory = "/home/slotis/public_html/logs/sicam/";
my $xml_log_prefix = "sicam";
my $xml_file;

# More variables.
my $debug = 1;
my $str;
my $error_str;

# Define time variables
my $now;
my ( @Now, $Year, $Month, $Day, $Hour, $Minute, $Second );
my $start_exp_time;
my $end_exp_time;
my $exposure;

# The commands read from of the scheduler.
my %cmd;

# The status variables of the SI camera - will be sent to the status server.
my %status;

# status variables from status server
my %data;

my $sock = undef;

my $tries = 0;

my $cmd_shell = undef;

my $sicam_cmd;
my $reply;
my $my_cmd;

#JACOB: every time is shows $status{ } ....that is something that is on the status server
# Spawn the sicam shell.
# The time when this program was last restarted.
$status{"sicam_restarted"} = localtime();
$status{"sicam_count"} = 0;

# GGW 3/30/2007 - Set camera status to idle.
$status{"sicam_exposing"} = "idle";
chomp($status{"sicam_exposing"});
    

# Spawn the sicam shell.
$cmd_shell = Expect->spawn( $sicam_shell )
  or die "Couldn't start program: $!\n";

# Prevent the program's output from being shown on our STDOUT
# We may want to comment this out when debugging.
$cmd_shell->log_stdout(0);

#JACOB: sinit is a command to initialize the camera software
#
# This is the beginning of commands to initialize the SICAM.
$my_cmd = "sinit -f\n";
print "sicam command: $my_cmd\n";
log_xml ("cmd", $my_cmd);
$cmd_shell->send( $my_cmd );

# This is sort of a hack that is special to the sinit command.  
# We wait for the second prompt to return, then everything seems okay.
my @rtn_status = $cmd_shell->expect( 5, '; ');
log_xml ("reply", $rtn_status[3]);
print "sicam reply:  $rtn_status[3]\n";
@rtn_status = $cmd_shell->expect( 5, '; ');
log_xml ("reply", $rtn_status[3]);
print "sicam reply:  $rtn_status[3]\n";

if (1) {
  # We may need to do a "sinit" a second time, but this seemed to always work
  # on March 7, 2005.  
  
  sleep 5;
  
  # This is the second sinit() for the SICAM.
  $my_cmd = "sinit\n";
  print "sicam command: $my_cmd\n";
  log_xml ("cmd", $my_cmd);
  $cmd_shell->send( $my_cmd );
  
  # This is sort of a hack that is special to the sinit command.  
  # We wait for the second prompt to return, then everything seems okay.
  @rtn_status = $cmd_shell->expect( 5, '; ');
  log_xml ("reply", $rtn_status[3]);
  print "sicam reply:  $rtn_status[3]\n";
  @rtn_status = $cmd_shell->expect( 5, '; ');
  log_xml ("reply", $rtn_status[3]);
  print "sicam reply:  $rtn_status[3]\n";
}

#JACOB: obviously, the camera didn't always start cleanly, so this part would 
# cycle power on the camera if the multiple sinit commands failed
#
# If the init doesn't work, try to turn the camera on again.
if ( 0 ) {
  
  if ($rtn_status[3] =~ /failed/) {
    system('/home/slotis/perl/galil/camera.pl off');
    sleep 15;
    system('/home/slotis/perl/galil/camera.pl on');
    sleep 15;
    
    $cmd_shell->hard_close() if $cmd_shell;
    
    # Spawn the sicam shell.
    # The time when this program was last restarted.
    $status{"sicam_restarted"} = localtime();
    $status{"sicam_count"} = 0;
    
    # Spawn the sicam shell.
    $cmd_shell = Expect->spawn( $sicam_shell )
      or die "Couldn't start program: $!\n";
    
    # Prevent the program's output from being shown on our STDOUT
    # We may want to comment this out when debugging.
    $cmd_shell->log_stdout(0);
    
    # This is the beginning of commands to initialize the SICAM.
    $my_cmd = "sinit -f\n";
    print "sicam command: $my_cmd\n";
    log_xml ("cmd", $my_cmd);
    $cmd_shell->send( $my_cmd );
    @rtn_status = $cmd_shell->expect( 5, '; ');
    log_xml ("reply", $rtn_status[3]);
    print "sicam reply:  $rtn_status[3]\n";
    
    # We may need to do a "sinit" a second time, but this seemed to always work
    # on March 7, 2005.  
    
    # This is sort of a hack that is special to the sinit command.  
    # We wait for the second prompt to return, then everything seems okay.
    $my_cmd = "sinit -f\n";
    print "sicam command: $my_cmd\n";
    log_xml ("cmd", $my_cmd);
    $cmd_shell->send( $my_cmd );
    my @rtn_status = $cmd_shell->expect( 5, '; ');
    log_xml ("reply", $rtn_status[3]);
    print "sicam reply:  $rtn_status[3]\n";
    
    if ($rtn_status[3] =~ /failed/) {
      my $fail_time = localtime();
      # I've failed twice to initialize the camera.  What to do?
  
#JACOB: we used to send emails to the key people when the camera client was started
# ...I'm not sure if I want to do this again, and if so what must be done for it to work    
      open (MAIL, "|/usr/sbin/sendmail -t");
      print MAIL "To: J Duane Gibson <jdgibson\@mmto.org>, Peter Milne <pmilne\@kaboom.as.arizona.edu>, Grant Williams <ggwilli\@mmto.org>, Peter Milne <pmilne511\@prodigy.net>\n";
      print MAIL "From: Slotis <slotis\@slotis.kpno.noao.edu>\n";
      print MAIL "Subject: SICAM camera failed to initialize\n\n";
      
      print MAIL "The SICAM camera failed to initialize at $fail_time\n";
      close (MAIL);
    }
  }    
}

# JACOB: here is where the configuration file is to be read in
# It set up things like how much of the CCD to read-out, gain, etc
$my_cmd = "install_config /home/slotis/sicam/config/slotis.set\n";
print "sicam command: $my_cmd\n";
log_xml ("cmd", $my_cmd);
$cmd_shell->send( $my_cmd );
@rtn_status = $cmd_shell->expect( 5, '; ');
log_xml ("reply", $rtn_status[3]);
print "sicam reply:  $rtn_status[3]\n";


# JACOB: writing 5 test files to verify that things are working
#
## 9/1/2005  JDG
## Changed from writing just one file, today.fits, to writing
## a series of five FITS files, today1.fits through today5.fits,
## all with the same exposure parameters and sleeps.
#
#

# I'm adding the taking of a short exposure to the startup sequence,
# just to see if everything works and the results are logged.


$exposure = 1;			# FITS EXPTIME keyword is in seconds.

$my_cmd = "setexp 1000\n";
print "sicam command: $my_cmd\n";
log_xml ("cmd", $my_cmd);
$cmd_shell->send( $my_cmd );
@rtn_status = $cmd_shell->expect( 5, '; ');
log_xml ("reply", $rtn_status[3]);
print "sicam reply:  $rtn_status[3]\n";

print localtime(time()) . "\n";

$my_cmd = "expose\n";
print "sicam command: $my_cmd\n";
log_xml ("cmd", $my_cmd);
$start_exp_time = strftime("%Y-%m-%dT%H:%M:%S", localtime);	
$cmd_shell->send( $my_cmd );
@rtn_status = $cmd_shell->expect( 5, '; ');
log_xml ("reply", $rtn_status[3]);
print "sicam reply:  $rtn_status[3]\n";

# Remove the today1.fits file if it exists.
system("rm today1.fits");

# This is important!!!  You need to sleep for 15 seconds or so after
# the first expose command and _before_ you do a wfits command!!!
sleep 15;


$my_cmd = "wfits today1.fits\n";
print "sicam command: $my_cmd\n";
log_xml ("cmd", $my_cmd);
$cmd_shell->send( $my_cmd );
@rtn_status = $cmd_shell->expect( 5, '; ');
log_xml ("reply", $rtn_status[3]);
print "sicam reply:  $rtn_status[3]\n";

print localtime(time()) . "\n";
$end_exp_time = strftime("%Y-%m-%dT%H:%M:%S", localtime);

write_header2("today1.fits");


#moved cooler on to this placement (originally after install_config)
# 3-12-07 PAM
$my_cmd = "cooler on\n";
print "sicam command: $my_cmd\n";
log_xml ("cmd", $my_cmd);
$cmd_shell->send( $my_cmd );
@rtn_status = $cmd_shell->expect( 5, '; ');
log_xml ("reply", $rtn_status[3]);
print "sicam reply:  $rtn_status[3]\n";

# I'm adding the taking of a short exposure to the startup sequence,
# just to see if everything works and the results are logged.
$exposure = 1;			# FITS EXPTIME keyword is in seconds.
$my_cmd = "setexp 2000\n";
print "sicam command: $my_cmd\n";
log_xml ("cmd", $my_cmd);
$cmd_shell->send( $my_cmd );
@rtn_status = $cmd_shell->expect( 5, '; ');
log_xml ("reply", $rtn_status[3]);
print "sicam reply:  $rtn_status[3]\n";

print localtime(time()) . "\n";

$my_cmd = "expose\n";
print "sicam command: $my_cmd\n";
log_xml ("cmd", $my_cmd);
$start_exp_time = strftime("%Y-%m-%dT%H:%M:%S", localtime);	
$cmd_shell->send( $my_cmd );
@rtn_status = $cmd_shell->expect( 5, '; ');
log_xml ("reply", $rtn_status[3]);
print "sicam reply:  $rtn_status[3]\n";

# Remove the today2.fits file if it exists.
system("rm today2.fits");

# This is important!!!  You need to sleep for 15 seconds or so after
# the first expose command and _before_ you do a wfits command!!!
sleep 15;

$my_cmd = "wfits today2.fits\n";
print "sicam command: $my_cmd\n";
log_xml ("cmd", $my_cmd);
$cmd_shell->send( $my_cmd );
@rtn_status = $cmd_shell->expect( 5, '; ');
log_xml ("reply", $rtn_status[3]);
print "sicam reply:  $rtn_status[3]\n";

print localtime(time()) . "\n";
$end_exp_time = strftime("%Y-%m-%dT%H:%M:%S", localtime);

write_header2("today2.fits");


# I'm adding the taking of a short exposure to the startup sequence,
# just to see if everything works and the results are logged.
$exposure = 1;			# FITS EXPTIME keyword is in seconds.
$my_cmd = "setexp 3000\n";
print "sicam command: $my_cmd\n";
log_xml ("cmd", $my_cmd);
$cmd_shell->send( $my_cmd );
@rtn_status = $cmd_shell->expect( 5, '; ');
log_xml ("reply", $rtn_status[3]);
print "sicam reply:  $rtn_status[3]\n";

print localtime(time()) . "\n";

$my_cmd = "expose\n";
print "sicam command: $my_cmd\n";
log_xml ("cmd", $my_cmd);
$start_exp_time = strftime("%Y-%m-%dT%H:%M:%S", localtime);	
$cmd_shell->send( $my_cmd );
@rtn_status = $cmd_shell->expect( 5, '; ');
log_xml ("reply", $rtn_status[3]);
print "sicam reply:  $rtn_status[3]\n";

# Remove the today3.fits file if it exists.
system("rm today3.fits");

# This is important!!!  You need to sleep for 15 seconds or so after
# the first expose command and _before_ you do a wfits command!!!
sleep 15;

$my_cmd = "wfits today3.fits\n";
print "sicam command: $my_cmd\n";
log_xml ("cmd", $my_cmd);
$cmd_shell->send( $my_cmd );
@rtn_status = $cmd_shell->expect( 5, '; ');
log_xml ("reply", $rtn_status[3]);
print "sicam reply:  $rtn_status[3]\n";

print localtime(time()) . "\n";
$end_exp_time = strftime("%Y-%m-%dT%H:%M:%S", localtime);

write_header2("today3.fits");



# I'm adding the taking of a short exposure to the startup sequence,
# just to see if everything works and the results are logged.
$exposure = 1;			# FITS EXPTIME keyword is in seconds.
$my_cmd = "setexp 4000\n";
print "sicam command: $my_cmd\n";
log_xml ("cmd", $my_cmd);
$cmd_shell->send( $my_cmd );
@rtn_status = $cmd_shell->expect( 5, '; ');
log_xml ("reply", $rtn_status[3]);
print "sicam reply:  $rtn_status[3]\n";

print localtime(time()) . "\n";

$my_cmd = "expose\n";
print "sicam command: $my_cmd\n";
log_xml ("cmd", $my_cmd);
$start_exp_time = strftime("%Y-%m-%dT%H:%M:%S", localtime);	
$cmd_shell->send( $my_cmd );
@rtn_status = $cmd_shell->expect( 5, '; ');
log_xml ("reply", $rtn_status[3]);
print "sicam reply:  $rtn_status[3]\n";

# Remove the today4.fits file if it exists.
system("rm today4.fits");

# This is important!!!  You need to sleep for 15 seconds or so after
# the first expose command and _before_ you do a wfits command!!!
sleep 15;

$my_cmd = "wfits today4.fits\n";
print "sicam command: $my_cmd\n";
log_xml ("cmd", $my_cmd);
$cmd_shell->send( $my_cmd );
@rtn_status = $cmd_shell->expect( 5, '; ');
log_xml ("reply", $rtn_status[3]);
print "sicam reply:  $rtn_status[3]\n";

print localtime(time()) . "\n";
$end_exp_time = strftime("%Y-%m-%dT%H:%M:%S", localtime);

write_header2("today4.fits");

# I'm adding the taking of a short exposure to the startup sequence,
# just to see if everything works and the results are logged.
$exposure = 1;			# FITS EXPTIME keyword is in seconds.
$my_cmd = "setexp 5000\n";
print "sicam command: $my_cmd\n";
log_xml ("cmd", $my_cmd);
$cmd_shell->send( $my_cmd );
@rtn_status = $cmd_shell->expect( 5, '; ');
log_xml ("reply", $rtn_status[3]);
print "sicam reply:  $rtn_status[3]\n";

print localtime(time()) . "\n";

$my_cmd = "expose\n";
print "sicam command: $my_cmd\n";
log_xml ("cmd", $my_cmd);
$start_exp_time = strftime("%Y-%m-%dT%H:%M:%S", localtime);	
$cmd_shell->send( $my_cmd );
@rtn_status = $cmd_shell->expect( 5, '; ');
log_xml ("reply", $rtn_status[3]);
print "sicam reply:  $rtn_status[3]\n";

# Remove the today5.fits file if it exists.
system("rm today5.fits");

# This is important!!!  You need to sleep for 15 seconds or so after
# the first expose command and _before_ you do a wfits command!!!
sleep 15;

$my_cmd = "wfits today5.fits\n";
print "sicam command: $my_cmd\n";
log_xml ("cmd", $my_cmd);
$cmd_shell->send( $my_cmd );
@rtn_status = $cmd_shell->expect( 5, '; ');
log_xml ("reply", $rtn_status[3]);
print "sicam reply:  $rtn_status[3]\n";

print localtime(time()) . "\n";
$end_exp_time = strftime("%Y-%m-%dT%H:%M:%S", localtime);

write_header2("today5.fits");

#JACOB: the test exposure have been completed and we are now ready to 
# loop while waiting for scheduler commands
#
# Endless loop of reading from scheduler and executing SICAM commands.
my $do_loop = 1;

MAINLOOP: while ($do_loop) {
    
  $status{"sicam_count"} += 1;

  update_status_server();
    
  # Using the select function for a finer resolution than sleep() allows.
  # Currently running this loop at 2Hz.
  select undef, undef, undef, 0.5;
    
  read_cmds();
    
  process_cmds();

  # GGW 10/5/2006 - Added the RRD tool updating.
  # Need to check to see if the count is a factor of 120 or 60-sec then update the RRD
  # GGW 10/27/2009 - typo was looking in $data rather than $status for the last two variables
  if ( $status{'sicam_count'}%120 == 0 ) {
      if (  defined($status{'sicam_backplatetemp'}) && defined($status{'sicam_ccdtemp'}) && defined($status{'sicam_vacuum'}) ) {
          RRDs::update ("/home/slotis/rrd/sicam.rrd", "--template=sicam_backplatetemp:sicam_ccdtemp:sicam_vacuum", "N:$status{'sicam_backplatetemp'}:$status{'sicam_ccdtemp'}:$status{'sicam_vacuum'}");
         my $ERR=RRDs::error;
#         print localtime() . ": ERROR while updating sicam.rrd: $ERR\n" if $ERR;
      }
  }

  # GGW 3/30/2007 - added camera status.
  # Need to check to see if the count is a factor of 120 or 60-sec then get  
  # the camera status.  This is separate from the RRD loop in case we want
  # to change timing at a later date.
  if ( $status{'sicam_count'}%120 == 0 ) {
    if ( $status{'sicam_exposing'} eq "idle" ) {

      # GGW 3/30/2007 - Removed get_status query since camstat or timeleft hang
      # the camera with the new firmware if done during readout.  A real fix
      # which avoid getting status during an exposure will be implemented.
      # Spectral Instruments also strongly advises against querying camera
      # status as often as 2 Hz they suggest minutes.
      # GGW 3/30/2007 - uncommented and put in the 1 minute loop.
      # GGW 4/2/2007 - commented out again.
      # GGW 10/27/2009 - trying to uncomment based on idle in the sicam_exposing status variable
      get_status();

    }
  }

    
}

# Never get here.  We may need to close the sicam shell another way.
$cmd_shell->hard_close();

# Read the camera status. 
#JACOB: note that "camstat" was the SI camera command to display the status of all variables
sub get_status {
    
  # First, testing for the timeleft (in seconds).
  # GGW 03/28/2007 - Took out the camstat command because it hangs the 
  # camera if sent during a readout.  The scheme for getting cameras 
  # status needs to be reworked with the new SI Camera firmware.
  # GGW 03/30/2007 - Put the camstat back in because the timeleft also
  # hung the camera.  We'll just remove the call to get_status above
  # and implement a real fix later. 
  $sicam_cmd = "timeleft ;camstat";
    
  $reply = do_sicam_cmd ( $sicam_cmd, 43200 ); 
  
  if ( defined ( $reply ) && $reply =~ /^\s*(\d+)/ ) {
    $status{"sicam_timeleft"} = $1;
    chomp($status{"sicam_timeleft"});
  }
  
  if ( defined ( $reply ) &&  $reply =~ /ccdtemp=\s*(\d*)/ ) {
    $status{"sicam_ccdtemp"} = $1;
    chomp($status{"sicam_ccdtemp"});
  }
	
  if (  defined ( $reply ) && $reply =~ /backplatetemp=\s*(\d*)/ ) {
    $status{"sicam_backplatetemp"} = $1;
    chomp($status{"sicam_backplatetemp"});
  }
  
  if ( defined ( $reply ) && $reply =~ /vacuum=\s*([\.\d]*)/ ) {
    $status{"sicam_vacuum"} = $1;
    chomp($status{"sicam_vacuum"});
  }   
  
  if ( defined ( $reply ) && $reply =~ /shutter=\s*([\.\d]*)/ ) {
    $status{"sicam_shutter"} = $1;
    chomp($status{"sicam_shutter"});
  }
  
  if ( defined ( $reply ) && $reply =~ /xirq=\s*([\.\d]*)/ ) {
    $status{"sicam_xirq"} = $1;
    chomp($status{"sicam_xirq"});
  }   
  $status{"sicam_update"} = localtime();
}

# Read the commands on the SLotis command scheduler, slotis:5134.
#JACOB: this is the scheduler on the main computer
sub read_cmds {
  # Clear out the commands hash.
  %cmd = ();
  $now = time();

  # Open a socket to the scheduler socket server.
  $sock = Net::Telnet->new (Host => $slotis_scheduler_host,
			    Port =>  $slotis_scheduler_port,
			    Errmode => "return",
			    Timeout => 1,
			    Binmode => 1,  
			    Telnetmode => 0
			   );
    
  # Read all of the commands off of the scheduler.
  if ( $sock ) {
    $sock->put("all\n");
	
    while ( 1 ) {
      # This getline() uses the timeout and modes of the Telnet->new().
      my $line = $sock->getline();
	    
      # Exit on error.
      last if $sock->errmsg() ne "";
      # Exit if we couldn't read anything (could be eof).
      last if ! defined ($line);
      # Normal exit with all data read.
      last if $line =~ /EOF/; 
	    
      # print "Returned: $line" if $debug;
      # Each line should have a Unix timestamp followed by a space, then the command.
      # Adding the ability to have fractions of a second, i.e., [\d\.]+.
      # So that several commands can be scheduled for a second.
      if ( $line =~ /([\d\.]+)\s+(.*)/ ) {
	my $key = $1;
	my $value = $2;
	# Removing trailing newlines.
	chomp ($value);
	# Removing ctrl-M's, if present.
	$value =~ s/\cM//g;
	$cmd{$key} = $value;
	last if $key > $now;
      }
    }
  }
    
  $sock->close() if $sock;
  undef($sock);

}


sub do_sicam_cmd {
  my @rtn_status;

  my ($my_cmd, $wait_time) = @_;
  chomp($my_cmd);

  # $cmd_shell->send( "\n" );
  # my @rtn_status = $cmd_shell->expect( $wait_time, ';');   
  print "sicam command: $my_cmd\n" unless $my_cmd =~ /timeleft/;
  # print "sicam command: $my_cmd\n";

  $cmd_shell->send( "$my_cmd\n" );
    
  # This was changed from 60 seconds wait time to a very large (12 hrs) wait time. JDG  7/15/2006
  # Could use "undef", but 12 hours = 43200 seconds.
  @rtn_status = $cmd_shell->expect( $wait_time, '; ');   
  
  # The return status is in the forth element of the returned array.
  if ( defined ( $rtn_status[3] ) ) {
    $reply = $rtn_status[3];
  } else {
    $reply = "???";
  }    

  # $cmd_shell->clear_accum();
  
  # print "sicam reply:  $reply\n" unless $my_cmd =~ /timeleft/;
  print "sicam reply:  $reply\n";
  return $reply;
}


# Sends the status information to the status server.
sub update_status_server {
    
  # The command to set a value in the status server.
  my $cmd = "";

  # Connect to the status socket server.
  $sock = Net::Telnet->new (Host => $slotis_status_server_host,
			    Port =>  $slotis_status_server_port,
			    Errmode => "return",
			    Timeout => 1,
			    Binmode => 1,  
			    Telnetmode => 0
			   );
    
  foreach my $key ( sort keys %status ) {
	
    if ( $sock ) {
      $cmd = "set_eof $key $status{$key}\n";
      my $rtn_status = $sock->put($cmd) if length($cmd) > 0;
      # put() returns 1 if all data was successfully written.
      # print $cmd if $debug;
      print "Error writing to status server" unless $rtn_status;
      # Returning the .EOF
      $sock->getline();
    }
  }	
    
  $sock->close() if $sock;
  undef($sock);
    
}

sub do_cmd {

  my ($my_cmd, $host, $port) = @_;
    
  $sock = Net::Telnet->new (Host => $host,
			    Port =>  $port,
			    Errmode => "return",
			    Timeout => 1,
			    Binmode => 1,
			    Telnetmode => 0
			   );
    
    
  if ( $sock ) {
    # This is non-blocking after the timeout.  Also, we are
    # not doing a "waitfor" any sort of reply.
    # Note:  Telnet->print() adds a newline.
    $sock->print( $my_cmd);
    # print $my_cmd . "\n" if $debug;
    $sock->close();
	
  }
}

# Executes any SLOTIS SICAM commands that are ready to be executed.
sub process_cmds {
  # Get the current Unix timestamp.
  $now = time();

  # Process all of the commands received from the scheduler in sorted order.
  foreach my $key ( sort keys %cmd ) { 

    # No need to continue if the Unix timestamp is in the future.
    if ( $key > $now ) {
      last;
    }
	
    # See if the command is for the SICAM system.  It will start with "SLOTIS SICAM".
    if ( $cmd{$key} =~ /^SLOTIS SICAM\s+(.*)/ ) {
      $sicam_cmd = $1;
      my $reply = "";
      # I don't know about timing issues here.  We need to wait for
      # a command to finish successfully before doing the next command.
      # For now, I'm just letting the timestamp determine when
      # commands go to the SICAM.
	    
      # New SICAM command, "wfits_exp", that does an exposure and FITS file write,
      # including header information, as a single command.
      # Parameters for this command:
      # 1) The exposure time (in milliseconds) is the first parameter.
      # 2) The FITS file name is the second parameter (needs to be a valid Perl 'word').  
      # JDG 7/10/2005
      if ( $sicam_cmd =~ /^wfits_exp\s+(\d+)\s+([\/\w]+)\/([\.\w]+)/ ) {
	my $exposure_milliseconds = $1;
	my $exposure_seconds = $exposure_milliseconds/1000;
	my $fits_directory = $2;
	my $fits_filename = $3;
	 	
	my $timing_str = "Executed: " . localtime() . ", scheduled: " . localtime($key) . "; key = $key, cmd = $my_cmd\n";

	log_xml( "cmd", "$timing_str" );
		
	# Putting timeleft seconds on status server immediately before starting exposure commands.
	$status{"sicam_exposure"} = $exposure_seconds; 
	$status{"sicam_timeleft"} = $exposure_seconds;
	$status{"sicam_update"} = localtime();
	update_status_server();
		
	# Setting up SICAM exposure
	my $my_cmd = "setexp $exposure_milliseconds ;expose";
	$exposure = $exposure_seconds;
	
	$timing_str = "Executed: " . localtime() . ", scheduled: " . localtime($key) . "; key = $key, cmd = $my_cmd\n";
	
        $start_exp_time = strftime("%Y-%m-%dT%H:%M:%S", localtime);
	$reply = do_sicam_cmd( "$my_cmd", 43200 );
	log_xml( "reply/ execution info", "$reply/ $timing_str" );
		
	# Counting down the exposure in a while() loop.
	# The camera is doing its exposure at the same time.
	my $tick = 0;
	while ($tick < $exposure_seconds) {
	  sleep 1;
	  $tick++;
	  $status{"sicam_timeleft"} = $exposure_seconds - $tick;
	  $status{"sicam_update"} = localtime();
	  update_status_server();
	}
		
	# Writing the FITS file, this will block.
	$fits_filename = get_fits_filename() if $fits_filename =~ /now/;
	$my_cmd = "wfits " . $fits_directory . '/' . $fits_filename;
	$timing_str = "Executed: " . localtime() . ", scheduled: " . localtime($key) . "; key = $key, cmd = $my_cmd\n";
	$reply = do_sicam_cmd( "$my_cmd", 43200 );
	log_xml( "reply/ execution info", "$reply/ $timing_str" );
		
	# Adding header info to FITS file.
	$end_exp_time = strftime("%Y-%m-%dT%H:%M:%S", localtime);
	write_header2($fits_directory . '/' . $fits_filename);
	
	$status{"sicam_exposure"} = 0;
	# timeleft will be set to zero in the normal monitoring of camera status.

      } elsif ( $sicam_cmd =~ /object\s+(.*)/ ) {
	$status{'object'} = $1;
	chomp($status{'object'});
	
      } else {
	# Regular SICAM command, including IM_ALIVE.
	# log_xml( "cmd", $cmd{$key});
	# Getting the time at the time of execution, not after.
	my $timing_str = "Executed: " . localtime() . ", scheduled: " . localtime($key) . "; key = $key, cmd = $sicam_cmd\n";
	$start_exp_time = strftime("%Y-%m-%dT%H:%M:%S", localtime) if $sicam_cmd =~ /expose/;
        # GGW 3/30/2007 - Update status to indicate exposure is underway.
        $status{"sicam_exposing"} = "exposing" if ($sicam_cmd =~ /expose/ || $sicam_cmd =~ /dark/);
        chomp($status{"sicam_exposing"});

	# GGW 12/11/2008 - Replacing the following line with a block to 
	# update the exposure time on the status server

	#$exposure = $1/1000 if $sicam_cmd =~ /setexp\s+(\d+)/;

	if ( $sicam_cmd =~ /setexp\s+(\d+)/ ) {
	  $exposure = $1/1000;
	  $status{"sicam_exposure"} = $exposure; 
	  $status{"sicam_update"} = localtime();
	}
	
	$reply = do_sicam_cmd( "$sicam_cmd", 43200 ) unless $sicam_cmd =~ /IM_ALIVE/;
	log_xml( "reply/ execution info", "$reply/ $timing_str" );
		
	# New code to end this set of processing commands if we do a wfits command.
	# The commands will be read again from the scheduler.
	# JDG 7/15/2005 
	if ( $sicam_cmd =~ /wfits\s+([\/\w]+)\/([\.\w]+)/ ) {
	  my $fits_directory = $1;
	  my $fits_filename = $2;
	  $fits_filename = get_fits_filename() if $fits_filename =~ /now/;
	  $end_exp_time = strftime("%Y-%m-%dT%H:%M:%S", localtime);
	  write_header2($fits_directory . '/' .  $fits_filename);
          # GGW 3/30/2007 - Update the exposing status to identify completion.
          $status{"sicam_exposing"} = "idle";
          chomp($status{"sicam_exposing"});
	}
	
      }
	    
      # Clear the command off of the scheduler
      print "Clearing command $key\n" if $debug;
      do_cmd("set $key",  $slotis_scheduler_host,  $slotis_scheduler_port);
    }
  }
}


sub get_fits_filename {

  my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime();
    
  $year += 2000;
  $mon += 1;
  $mon = "0$mon" if $mon < 10;
  $mday = "0$mday" if $mday < 10;
  $hour = "0$hour" if $hour < 10;
  $min = "0$min" if $min < 10;
  $sec = "0$sec" if $sec < 10;
    
  # This is a unique FITS filename, down to the second.  We just need
  # to make sure that we call it after >1 second.
  return $year . "_" . $mon . "_" . $mday . "_" . $hour . "_" . $min . "_" . $sec . '.fits'; 
}
sub log_xml {

  my ( $log_type, $log_msg ) = @_;
    
  my $new_entry;
  my @lines;
    
  chomp($log_type);
  chomp($log_msg);
    
  $xml_file = get_xml_filename();
    
  # Open XML background log file in read only mode.
  if ( open( FD, "< $xml_file" ) ) { 
    @lines = <FD>;
    close FD;
  } else {
    #XML file doesn't exist.
    print "Couldn't open $xml_file.  Will try to create a new XML file.\n";
  }
    
  # Open in write (not append) mode.
  open( FD, "> $xml_file" ) or die ("Couldn't open $xml_file\n");
    
  # Process on lines in XML file:
  # Delete any entries more the 48 hours old
  # Add new entry.
  # etc.
    
  my $num_xml_entries = @lines;
    
  # Indicate an XML file and the XSL stylesheet.
  my $str =  "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n";
  print FD $str;
  $str = "<?xml-stylesheet type=\"text/xsl\" href=\"sicam.xsl\"?>\n";
  print FD  $str;
    
  # Add start root tag.
  print FD "<entries>\n";
    
  for ( my $i = 0; $i < $num_xml_entries; $i++ ) {
	
    if ( ( $lines[$i] =~ /<entries>/ ) || ( $lines[$i] =~ /<\/entries>/ ) ||  ( $lines[$i] =~ /<\?xml/ ) ) {
      # Don't write these lines.
    } else {
      print FD $lines[$i];
    }
  }
    
  # Add new entry.
  $new_entry = get_msg_xml( $log_type, $log_msg );
  print FD $new_entry;
    
  # Add end root tag.
  print FD "</entries>\n";
    
  close FD;

}


sub get_msg_xml {
    
  # $xml_type should be either "current" or "bg_log"
  my ( $log_type, $log_msg ) = @_;
    
  $now = time();
  my $date = localtime($now);
    
  read_status();

  my $xml = "    <entry timestamp=\"$now\" date=\"$date\" type=\"$log_type\" msg=\"$log_msg\"";

  foreach my $key ( sort keys %data ) { 
    $xml .= " " . $key . "=\"" . $data{$key} . "\"";
  }

  $xml .= "/>\n";
    
  return $xml;
    
}


# Only used internally to determine the log file name.
sub get_xml_filename {
    
  # Subtracting 12 hours (== 43200 seconds) from the number of seconds since the epoch.
  # This causes filenames to change at noon instead of midnight.
  @Now = localtime(time() - 43200);
  $Year = $Now[5]+2000;
  $Month = $Now[4]+1;
  $Month = "0$Month" if $Month < 10;
  $Day = $Now[3];
  $Day = "0$Day" if $Day < 10;
  my $str = $xml_log_directory . $xml_log_prefix . "_" . $Year . $Month . $Day . ".xml";
  # print $str . "\n" if $debug;
  return $str;
}


# Read the current status
sub read_status {
    
  # Open a socket to the scheduler socket server.
  $sock = Net::Telnet->new (Host => $slotis_status_server_host,
			    Port =>  $slotis_status_server_port,
			    Errmode => "return",
			    Timeout => 1,
			    Binmode => 1,  
			    Telnetmode => 0
			   );
    
  # Read all of the commands off of the scheduler.
  if ( $sock ) {
    $sock->put("all\n");
	
    while ( 1 ) {
      # This getline() uses the timeout and modes of the Telnet->new().
      my $line = $sock->getline();
	    
      # Exit on error.
      last if $sock->errmsg() ne "";
      # Exit if we couldn't read anything (could be eof).
      last if ! defined ($line);
      # Normal exit with all data read.
      last if $line =~ /EOF/; 
	    
      if ( $line =~ /(\w+)\s+(.+)/ ) {
	my $key = $1;
	my $value = $2;
	# Removing trailing newlines.
	chomp ($value);
	# Removing ctrl-M's, if present.
	$value =~ s/\cM//g;
	$data{$key} = $value;
      }
    }
  }
    
  $sock->close() if $sock;
  undef($sock);

}

# GGW 6/23/2005 - added the following subroutine.  I may need to change the
# variable status since update_status also uses it.

# GGW 6/23/2005 - Added the following subroutine
# Writes a fits header to a file.
#
# Commented out for now.  Try using write_header2instead. JDG  8/11/2005
#
# sub write_header {
#
#    my ($my_headfile) = @_;
#    my $fptr;
#    # Changed Grant's $status to $fits_status so that it has a different 
#    # name than the global status hash of sicam variables.  JDG  7/10/2005
#    #
#    my $fits_status;
#
#    #fits_create_file($fptr,$newfits_filename,$fits_status);
#    fits_create_file($fptr,'./temp.fits',$fits_status);
#    $fptr->update_key_str('DATASEC','[55:5,2090:2045]','',$fits_status);
#    $fptr->update_key_str('CCDSEC','[51:2098,1:2048]','',$fits_status);
#    $fptr->update_key_str('BIASSEC','[2105:2195,5:2195]','',$fits_status);
#    $fptr->update_key_str('TRIMSEC','[55:5,2090:2045]','',$fits_status);
#    $fptr->update_key_fixflt('GAIN',2.0,2,'gain, electrons per ADU',$fits_status);
#    $fptr->update_key_fixflt('RDNOISE',9.0,2,'1000 kHz read noise, electrons',$fits_status);
#    $fptr->update_key_str('OBSERVAT','Steward KP, Super-LOTIS','observatory',$fits_status);
#    $fptr->update_key_str('TELESCOP','Super-LOTIS 60-cm','telescope',$fits_status);
#    $fptr->update_key_str('OBSERVER','Hye-Sook Park, LLNL','PI',$fits_status);
#
# }

# JDG 7/10/2005 - Modified to write header after image was taken.
# Writes a fits header into an existing FITS file.  
sub write_header2 {
    
  my ($my_fitsfile) = @_;
  my $fptr;
  my $fits_status = 0;
  my $count = 0;
  my $dec_decimal;

  # We need to wait for the FITS file to be written to disk.
  while ( ( ! -e $my_fitsfile) && ($count < 30) ) {
    sleep 1;
    $count++;
    print "sleeping at $count...\n";
    # Return if the FITS file doesn't exist.
    return if $count >= 29;
  }

  # Just making sure that the file really, really exists.
  system("ls -l $my_fitsfile");

  print "Adding header info to $my_fitsfile\n";
    
  # Reading the TCS status so that we have current information for the FITS header.
  read_status();
    
  # Changed Grant's $status to $fits_status so that it has a different 
  # name than the global status hash of sicam variables.  JDG  7/10/2005
  #
  #fits_create_file($fptr,$newfits_filename,$fits_status);
  fits_open_file($fptr,$my_fitsfile,Astro::FITS::CFITSIO::READWRITE(),$fits_status);
  if ($fptr) {
    eval {
   
      # New keywords from Peter.  5/17/2019
      $fptr->update_key_str('BIASSEC','[1:50,1:2195]','bias section',$fits_status); 
      $fptr->update_key_str('TRIMSEC','[51:2098,1:2048]','trim section',$fits_status); 
      $fptr->update_key_str('DATASEC','[51:2098,1:2048]','actual data pixels in the raw frame',$fits_status); 
      $fptr->update_key_str('CCDSEC','[1:2048,1:2048]','section of the detector used',$fits_status); 
      $fptr->update_key_str('ORIGSEC','[1:2048,1:2048]','original size of the detector',$fits_status);

#      # New keywords from Grant.  9/11/2005
#      $fptr->update_key_str('BIASSEC','[2105:2195,5:2195]','bias section',$fits_status); 
#      $fptr->update_key_str('TRIMSEC','[51:2098,1:2048]','trim section',$fits_status); 
#      $fptr->update_key_str('DATASEC','[51:2098,1:2048]','actual data pixels in the raw frame',$fits_status); 
#      $fptr->update_key_str('CCDSEC','[1:2048,1:2048]','section of the detector used',$fits_status); 
#      $fptr->update_key_str('ORIGSEC','[1:2048,1:2048]','original size of the detector',$fits_status);

      # Previous version of these parameters.  This DATASEC is incorrect, and
      # doesn't work  work with DS9.
      #    $fptr->update_key_str('DATASEC','[55:5,2090:2045]','',$fits_status);
      #    $fptr->update_key_str('CCDSEC','[51:2098,1:2048]','',$fits_status);
      #    $fptr->update_key_str('BIASSEC','[2105:2195,5:2195]','',$fits_status);
      #    $fptr->update_key_str('TRIMSEC','[55:5,2090:2045]','',$fits_status);
      #    $fptr->update_key_str('ORIGSEC','[1:2200,1:2200]','Original size of full frame',$fits_status);

      # GGW - 2/13/2006
      # I modified the GAIN and RDNOISE settings to agree with measurements
      # made by Spectral Instruments and distributed to us:
      #  G = 3.93, RDN = 11.59 

      # $fptr->update_key_fixflt('GAIN',2.0,2,'gain, electrons per ADU',$fits_status);
      # $fptr->update_key_fixflt('RDNOISE',9.0,2,'1000 kHz read noise, electrons',$fits_status);
      $fptr->update_key_fixflt('GAIN',3.93,2,'gain, electrons per ADU',$fits_status);
      $fptr->update_key_fixflt('RDNOISE',11.59,2,'800 kHz read noise, electrons',$fits_status);
      $fptr->update_key_str('OBSERVAT','Steward KP, Super-LOTIS','observatory',$fits_status);
      $fptr->update_key_str('TELESCOP','Super-LOTIS 60-cm','telescope',$fits_status);
      # GGW 09/27/2007 - Changed observer from HSP to PAM
      #$fptr->update_key_str('OBSERVER','Peter A. Milne, U of A','PI',$fits_status);
      # GGW 09/14/2009 - Changed keyword from OBSERVER to INSTPI 
      # and PAM to GGW
      $fptr->update_key_str('INSTPI','P.A. Milne','PI',$fits_status);
    
      # This seems to cause a bus error if the FITS file doesn't already have a
      # EXPTIME keyword.
      my $exp_time = 0.0;
      my $exp_comment;
      $fptr->read_key_lng('EXPTIME',$exp_time,$exp_comment, $fits_status);
      $fptr->update_key_fixflt('EXPTIME',$exp_time/1000,3,'[s] exposure time in seconds',$fits_status);

      # Calculate the decimal RA and Dec.
      my $ra_hh = substr $data{"tcs_ra"}, 0, 2;
      my $ra_mm = substr $data{"tcs_ra"}, 2, 2;
      my $ra_ss = substr $data{"tcs_ra"}, 4;
      my $ra_decimal = 15.0 * ($ra_hh + (($ra_mm + ($ra_ss/60.0))/60.0));

      my $dec_dd = substr $data{"tcs_dec"}, 0, 3;
      my $dec_mm = substr $data{"tcs_dec"}, 3, 2;
      my $dec_ss = substr $data{"tcs_dec"}, 5;
      if ( $dec_dd =~ /^-/ ) {
         $dec_decimal = $dec_dd - (($dec_mm + ($dec_ss/60.0))/60.0);
      } else {
         $dec_decimal = $dec_dd + (($dec_mm + ($dec_ss/60.0))/60.0);
      }    

      my $ra_hms = substr $data{"tcs_ra"}, 0, 2;
      $ra_hms .= ":";
      $ra_hms .= substr $data{"tcs_ra"}, 2, 2;
      $ra_hms .= ":";
      $ra_hms .= substr $data{"tcs_ra"}, 4;
       
      my $dec_dms = substr $data{"tcs_dec"}, 0, 3;
      $dec_dms .= ":";
      $dec_dms .= substr $data{"tcs_dec"}, 3, 2;
      $dec_dms .= ":";
      $dec_dms .= substr $data{"tcs_dec"}, 5;

      # New parameters in FITS header.  JDG 7/10/2005
      # Changed some keyword names to better fit FITS standard names.
      $fptr->update_key_str('TCS_RA', $data{"tcs_ra"},'',$fits_status) if defined($data{"tcs_ra"});
      $fptr->update_key_str('TCS_DEC', $data{"tcs_dec"},'',$fits_status) if defined($data{"tcs_dec"});
      $fptr->update_key_str('RA',$ra_hms,'',$fits_status) if defined($data{"tcs_ra"});
      $fptr->update_key_str('DEC',$dec_dms,'',$fits_status) if defined($data{"tcs_dec"});
      $fptr->update_key_str('UT',$data{"tcs_ut"},'TCS UT',$fits_status) if defined($data{"tcs_ut"});
      # $fptr->update_key_str('TIME-OBS',$start_exp_time,'',$fits_status);
      # $fptr->update_key_str('TIME-END',$end_exp_time,'',$fits_status);
      $fptr->update_key_str('AZ',$data{"tcs_az"},'',$fits_status) if defined($data{"tcs_az"});
      $fptr->update_key_str('EL',$data{"tcs_el"},'',$fits_status) if defined($data{"tcs_el"});
      $fptr->update_key_str('TELFOCUS',$data{"tcs_focus"},'',$fits_status) if defined($data{"tcs_focus"});
      $fptr->update_key_str('LST',$data{"tcs_lst"},'',$fits_status) if defined($data{"tcs_lst"});
      $fptr->update_key_str('HA',$data{"tcs_ha"},'',$fits_status) if defined($data{"tcs_ha"});
      $fptr->update_key_str('JD',$data{"tcs_jd"},'',$fits_status) if defined($data{"tcs_jd"});
      $fptr->update_key_str('TS',$data{"tcs_timestamp"},'',$fits_status) if defined($data{"tcs_timestamp"});
      $fptr->update_key_str('AIRMASS',$data{"tcs_airmass"},'',$fits_status) if defined($data{"tcs_airmass"});
      $fptr->update_key_str('EQUINOX',$data{"tcs_display_equinox"},'',$fits_status) if defined($data{"tcs_display_equinox"});
  
# Added 1/24/2006 PAM
   $fptr->update_key_str('RH',$data{"metone_rh"},'Relative Humidity',$fits_status) if defined($data{"metone_rh"});
  $fptr->update_key_str('AMBTEMPC',$data{"metone_temperatureC"},'Ambient Temperature (C)',$fits_status) if defined($data{"metone_temperatureC"});
  $fptr->update_key_str('AMBTEMPF',$data{"metone_temperatureF"},'Ambient Temperature (F)',$fits_status) if defined($data{"metone_temperatureF"});
   $fptr->update_key_str('WINDDIR',$data{"metone_wind_direction"},'Wind Direction',$fits_status) if defined($data{"metone_wind_direction"});
   $fptr->update_key_str('WINDSPD',$data{"metone_wind_speed"},'Wind Speed',$fits_status) if defined($data{"metone_wind_speed"});
   $fptr->update_key_str('ROOFSTAT',$data{"galil_roofopen_status"},'Open=0.0...Closed=1.0',$fits_status) if defined($data{"galil_roofopen_status"});
# end PAM
# Adding in Boltwood info 12/11/2017
#   $fptr->update_key_str('BLTWD_CL',$data{"boltwood_cloudCond"},'Boltwood Cloud',$fits_status) if defined($data{"boltwood_cloudCond"});
   $fptr->update_key_str('BLTWD_RH',$data{"boltwood_relativeHumidityPercentage"},'Boltwood Relative Humidity',$fits_status) if defined($data{"boltwood_relativeHumidityPercentage"});
   $fptr->update_key_str('BLTWD_WS',$data{"boltwood_windSpeed"},'Boltwood Wind Speed',$fits_status) if defined($data{"boltwood_windSpeed"});
   $fptr->update_key_str('BLTWD_CL',$data{"boltwood_skyMinusAmbientTemperature"},'Boltwood Sky - Ambient',$fits_status) if defined($data{"boltwood_skyMinusAmbientTemperature"});
#end PAM



      # Added 9/17/2005  JDG.
      $fptr->update_key_str('FILTER',$data{"slotis_filter"},'',$fits_status) if defined($data{"slotis_filter"});
      $fptr->update_key_str('INSTFILT',$data{"slotis_filter_number"},'',$fits_status) if defined($data{"slotis_filter_number"});
  
    #  Note:  New as of 8/24/2005, these are not in the client scripts yet.
      $fptr->update_key_str('FILTCODE',$data{"slotis_filtcode"},'',$fits_status) if defined($data{"slotis_filtcode"});
      # This will put a FITS header entry with a keywork of "OBJECT" into the FITS file.
      # "object" is set on the status server by sicam_client.pl with a command in the format of 
      # the "set 0 SLOTIS SICAM object SN2005dy".   JDG 9/23/2005
      $fptr->update_key_str('OBJECT',$data{"object"},'',$fits_status) if defined($data{"object"});
      $fptr->update_key_str('IMAGETYP',$data{"sicam_type"},'',$fits_status) if defined($data{"sicam_type"});
      $fptr->update_key_str('CCDSUM',$data{"sicam_binning"},'',$fits_status) if defined($data{"sicam_binning"});
      # GGW 10/22/2009 - Added WCS fits header values.
      $fptr->update_key_str('CTYPE1','RA---TAN','',$fits_status);
      $fptr->update_key_str('CTYPE2','DEC--TAN','',$fits_status);
      $fptr->update_key_lng('CRPIX1',1074,'',$fits_status);
      $fptr->update_key_lng('CRPIX2',1028,'',$fits_status);
      $fptr->update_key_fixflt('CDELT1',0.00013766,8,'',$fits_status);
      $fptr->update_key_fixflt('CDELT2',0.00013766,8,'',$fits_status);
      $fptr->update_key_fixflt('CROTA2',-0.500,3,'',$fits_status);
      $fptr->update_key_fixflt('CRVAL1',$ra_decimal,6,'',$fits_status) if defined($data{"tcs_ra"});
      $fptr->update_key_fixflt('CRVAL2',$dec_decimal,5,'',$fits_status) if defined($data{"tcs_dec"});
    };
    fits_close_file($fptr,$fits_status);

    # GGW 11/26/2007 - Added the change of owner and group for the fitsfile
    # NOTE the user slotis has uid=500 and gid=500
    my $filecnt = chown 500, 500, $my_fitsfile;

    # Copying FITS file to slotis.  Added 9/22/2005 JDG.
    # We need to have a path defined for the FITS file so that we 
    # dont' copy today1.fits.
    # Need to test this.
    # GGW 3/30/2007 - removed scp because it seemed to kill the sicam_client
    # when run from the command line if a password was required.
# uncommenting 6/19/07 .....will daytime test 6/20
# GGW 11/20/2009 - adding the sudo so that ownership is preserved.
#system("scp $my_fitsfile slotis.kpno.noao.edu:$my_fitsfile &") if $my_fitsfile =~ /\w+\//;
#system("sudo -u slotis scp $my_fitsfile slotis.kpno.noao.edu:$my_fitsfile &") if $my_fitsfile =~ /\w+\//;

# GGW 11/30/2009 - added to copy files to slwebscope
# GGW 12/3/2009 - I combined the scp commands.
my $my_fitsfile_base = basename($my_fitsfile);
my $my_fitsfile_dir = dirname($my_fitsfile);
#system("sudo -u slotis scp $my_fitsfile slwebscope.as.arizona.edu:/home/slotis/data/slnfo/$my_fitsfile_base &") if $my_fitsfile =~ /\w+\//;
#system("sudo -u slotis scp $my_fitsfile /home/slotis/flag.txt slwebscope.as.arizona.edu:/home/slotis/data/slnfo/ &") if $my_fitsfile =~ /\w+\//;

# GGW 11/20/2009 - send a flag for ATIS
# GGW 11/30/2009 - modified to send to slwebscope
#system("sudo -u slotis scp /home/slotis/flag.txt slwebscope.as.arizona.edu:/home/slotis/data/slnfo/flag.txt &") if $my_fitsfile =~ /\w+\//;

  } else {
    print "Couldn't open file $my_fitsfile\n";
    log_xml ("error", "Couldn't open file $my_fitsfile");
  }
}


# All Done.




