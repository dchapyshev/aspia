<?php
//
// SmartCafe Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

function getDownloadUrl($mysqli, $release_id, $package, $arch)
{
    $sql = "SELECT url
            FROM downloads
            WHERE release_id = '$release_id' AND package = '$package' AND arch = '$arch'";
    if (!$result = $mysqli->query($sql))
        die('Failed to execute database query: '.$mysqli->error);

    if ($result->num_rows == 0)
        die('Download url not found.');

    $row = $result->fetch_array();
    $result->close();

    return $row["url"];
}

function getUpdates($mysqli, $package, $version, $arch)
{
    $sql = "SELECT release_id FROM updates WHERE version = '$version'";
    if (!$result = $mysqli->query($sql))
        die('Failed to execute database query: '.$mysqli->error);

    if ($result->num_rows == 0)
        die('No updates available');

    $row = $result->fetch_array();
    $result->close();

    $release_id = $row["release_id"];

    $url = getDownloadUrl($mysqli, $release_id, $package, $arch);
    if (empty($url))
        die('Empty URL for download');

    $sql = "SELECT version, description FROM releases WHERE id = '$release_id'";
    if (!$result = $mysqli->query($sql))
        die('Failed to execute database query: '.$mysqli->error);

    if ($result->num_rows == 0)
        die('No releases available');

    $row = $result->fetch_array();
    $result->close();

    $target_version = $row["version"];
    $description = $row["description"];

    if (empty($target_version))
        die('Empty target version');

    $xml = xmlwriter_open_memory();
    xmlwriter_set_indent($xml, 1);
    xmlwriter_set_indent_string($xml, ' ');

    xmlwriter_start_document($xml, '1.0', 'UTF-8');
    xmlwriter_start_element($xml, 'update');

    xmlwriter_start_element($xml, 'version');
    xmlwriter_text($xml, $target_version);
    xmlwriter_end_element($xml); // version

    xmlwriter_start_element($xml, 'description');
    xmlwriter_text($xml, $description);
    xmlwriter_end_element($xml); // description

    xmlwriter_start_element($xml, 'url');
    xmlwriter_text($xml, $url);
    xmlwriter_end_element($xml); // url

    xmlwriter_end_element($xml); // update
    xmlwriter_end_document($xml);

    echo xmlwriter_output_memory($xml);
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
        die('Could not connect to database: '.$mysqli->connect_error);

    // Get the package name, version and arch from the query.
    $package = $mysqli->real_escape_string($query['package']);
    $version = $mysqli->real_escape_string($query['version']);
    $arch = $mysqli->real_escape_string($query['arch']);

    if (empty($package) || empty($version))
        die('Invalid request received.');

    // We only support 3 groups of digits in the version number.
    $pieces = explode(".", $version);
    while (count($pieces) > 3)
        array_pop($pieces);
    $version = implode(".", $pieces);

    // x86 by default.
    if (empty($arch))
        $arch = "x86";

    getUpdates($mysqli, $package, $version, $arch);
    $mysqli->close();
}

// Run the update check.
doWork();

?>