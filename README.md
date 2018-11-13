
# Overview of Meanwhile

Meanwhile is a library for connecting to an [IBM Sametime] (briefly
Lotus Instant Messaging, originally [VPBuddy]) community. It uses a
protocol based in part off of the [IMPP draft], and in part off of
traces of TCP sessions from existing clients.

[IBM Sametime]: https://en.wikipedia.org/wiki/IBM_Sametime

[VPBuddy]: https://en.wikipedia.org/wiki/Virtual_Places_Chat

[IMPP draft]: https://tools.ietf.org/html/draft-houri-sametime-community-client-00


## Origins

Work on Meanwhile first began in 2002. It wasn't untol 2004 that the
first working client connection was made. With that significant hurdle
overcome, Meanwhile remained in active development until 2007, when
the primary author changed jobs and lost regular access to a Sametime
deployment. A 1.1.0 release happened in 2008 based on submitted
third-party patches, and a final followup version 1.1.1 release was
created in 2015 completely external to the original project.

In 2018 the author discovered that he would potentially be returning
to an environment that used Sametime, and began migrating the old CVS
repository history from sourceforge onto github. The 1.1.1 changes
were merged on-top of that, and that leaves us where we are today.


## Contact

Author: Christopher (siege) O'Brien  <obriencj@gmail.com>

IRC Channel: #meanwhile on [Freenode]

Original Git Repository: <https://github.com/obriencj/meanwhile>

Defunct Sourceforge Project: <https://sourceforge.net/projects/meanwhile/>

[Freenode]: https://freenode.net


## License

This library is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation; either version 3 of the
License, or (at your option) any later version.

This library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, see
<http://www.gnu.org/licenses/>.
