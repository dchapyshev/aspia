Purpose
=======
These scripts are intended for the server part of the SmartCafe update system. They are installed on the web server.

Installation
------------
1. Your web server must support **PHP** and **MySQL**.
2. Copy the PHP files (**config.php**, **index.php**, **update.php**) from this directory to your web server.
3. Import the **database.sql** file into your database (for example, through phpMyAdmin).
4. Set the database connection parameters in file **config.php**.

Adding updates
--------------
1. Add a record with versions (new release versions) and description to table **releases**. Remember the ID of the added record.
2. Add records to table **downloads** for Host, Client and Console.
* In the **release_id** field, enter the release ID from step 1.
* In the **package** field, enter the name of the package. Valid values: **host**, **console**, **client**.
* In the **arch** field, enter the name of the architecture. Valid values: **x86**, **x86_64**.
* In the **url** field, provide a download link for the package with the specified name and architecture.
3. In table **updates**, add a record with the source version and the release ID from point 1.
