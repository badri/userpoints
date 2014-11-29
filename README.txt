
Copyright 2005-2008 http://2bits.com

Description
-----------
The userpoints and userpoints_nc module provides the ability for users to gain
points with the do certain actions, such as:

- posting a node (different points can be awarded for different
  node types, e.g. page, story, forum, image, ...etc.)
- posting a comment
- moderating a comment

Upon deleting a node or a comment the number of points is subtracted.
If a node or comment author is changed points are transferred respectively

The number of points for each of the above actions is configurable by
the site adminsitrator.

A transaction log is created for each event. The log is viewable by
the admin.

Points can be moderated, i.e. approval can be done by the admin at a later
time.

A block displays the number of points the user gained. Another block 
displays the top 5 users who earned points.

----
Using modules from the project http://drupal.org/project/userpoints_contrib 
point can be awarded for other actions. 
including:
- voting on a node (requires the nodevote module)
- referring a person to the site (requires referral module)
- a visitor comes to the site via clicking on an affiliate link
  (requires the affiliates module)
- voting up or down a node (requires the vote_up_down module)
- inviting a person to register on the site (requires invite module)
- invited person actually registers on the site
- purchasing from your e-commerce store (reward points)

Using real money, users can purchase points from your ecommerce store
as well. Moreover, the points can be used as currency for ecommerce as well,
as in a form of payment

This module is useful in providing an incentive for users to participate
in the site, and be more active. The module is easily extended through use of 
the API (see below)

Initially sponsored by: http://artalyst.com

Installation
------------
To install this module, do the following:

1. Extract the tar ball that you downloaded from Drupal.org.

2. Upload the userpoints directory and all its contents to your
   modules directory.

Configuration
-------------
To enable this module do the following:

1. Go to Admin -> Modules, and enable userpoints.
   Check the messages to make sure that you did not get any errors
   on database creation.

2. Go to Admin -> Configuration -> People -> userpoints.

   Configure the options as per your requirements

3. Go to Admin -> People -> Permissions and configure as desired.

API
---

The API documentation can be found at api.worldempire.ch/api/userpoints.


Bugs/Features/Patches:
----------------------
If you want to report bugs, feature requests, or submit a patch, please do so
at the project page on drupal.org.
http://drupal.org/project/userpoints

Author
------
Khalid Baheyeldin (http://baheyeldin.com/khalid and http://2bits.com)

The Drupal 7 version of this module are maintained by Berdir
(http://drupal.org/user/214652).

If you use this module, find it useful, and want to send the author
a thank you note, then use the Feedback/Contact page at the URL above.

The author can also be contacted for paid customizations of this
and other modules.
