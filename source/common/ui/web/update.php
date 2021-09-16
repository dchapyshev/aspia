<?php
//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

include 'config.php';

function getPackageId($mysqli, $package)
{
    $sql = "SELECT id FROM packages WHERE name='$package'";

    if (!$result = $mysqli->query($sql))
    {
        die('Failed to execute database query: '.$mysqli->error);
    }

    if ($result->num_rows == 0)
    {
        die('Package not found.');
    }

    $row = $result->fetch_array();
    $result->close();

    return $row["id"];
}

function getUpdates($mysqli, $package_id, $version)
{
    $pieces = explode(".", $version);

    // We only support 3 groups of digits in the version number.
    while (count($pieces) > 3)
        array_pop($pieces);

    $version = implode(".", $pieces);

    $sql = "SELECT target_version, description, url
            FROM updates
            WHERE package_id = '$package_id' AND source_version = '$version'";

    if (!$result = $mysqli->query($sql))
    {
        die('Failed to execute database query: '.$mysqli->error);
    }

    echo "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
    echo "<update>";

    if ($result->num_rows != 0)
    {
        // There is an update available.
        $row = $result->fetch_array();

        echo "<version>".$row['target_version']."</version>";
        echo "<description>".$row['description']."</description>";
        echo "<url>".$row['url']."</url>";
    }

    echo "</update>";

    $result->close();
}

function doWork()
{
    parse_str($_SERVER["QUERY_STRING"], $query);

    // Connect to the database.
    $mysqli = new mysqli(Config::$db_host,
                         Config::$db_user,
                         Config::$db_password,
                         Config::$db_name);
    if (mysqli_connect_errno())
    {
        die('Could not connect to database: '.$mysqli->connect_error);
    }

    // Get the package name and version from the query.
    $package = $mysqli->real_escape_string($query['package']);
    $version = $mysqli->real_escape_string($query['version']);

    if (empty($package) || empty($version))
    {
        die('Invalid request received.');
    }

    getUpdates($mysqli,
               getPackageId($mysqli, $package),
               $version);

    $mysqli->close();
}

// Run the update check.
doWork();

?>