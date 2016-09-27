<?php
// $Id$

/**
 * @file
 * Feeds Fetcher for ARTstor Shared Shelf API
 */

/**
 * Definition of the import batch object created on the fetching stage by
 * FeedsHTTPFetcher.
 */
class FeedsSharedshelfResult extends FeedsFetcherResult {
  protected $config;

  /**
   * Constructor.
   */
  public function __construct($config = NULL, $feed_nid) {
    $this->config = $config;
    parent::__construct('', $feed_nid);
  }
  /**
   * Create XML using string or array
   *
   * @param mixed $data input data
   * @param SimpleXMLElement $xml
   * @param string $child name of first level child
   *
   * @return adding Xml formated data into SimpleXmlElement
   *
   * from http://php.net/manual/en/simplexmlelement.addchild.php#111087
   */

  private function data2XML(array $data, SimpleXMLElement $xml, $child = "items") {
    foreach ($data as $key => $val) {
      if (is_array($val)) {

        if (is_numeric($key)) {
          $node  = $xml->addChild($child);
          $nodes = $node->getName($child);
        }
        else {

          $node  = $xml->addChild($key);
          $nodes = $node->getName($key);
        }

        $node->addChild($nodes, self::data2Xml($val, $node));
      }
      else {
        $xml->addChild($key, $val);
      }
    }

  }

  /**
   * convert returned array into xml
   */
  private function convert_to_xml($vals) {
    $xml = new SimpleXMLElement('<root/>');
    $this->data2XML($vals, $xml, "Items");
    $xml_text = $xml->asXML();
    //drupal_set_message($xml_text);
    return $xml_text;
  }

  /**
   * Implementation of FeedsImportBatch::getRaw();
   */
  public function getRaw() {
    $client =  $this->config['client'];
    $method_name =  $this->config['settings']['operations'];
    $project_id = $this->config['settings']['project_id'];
    $asset_id = $this->config['settings']['asset_id'];
    $thumbnail_size = $this->config['settings']['thumbnail_size'];
    dpm($this->config, "configuration getRaw");
    try {
      // TODO: $result is from json_decode - try http://drupal.org/project/feeds_jsonpath_parser
      $result = clients_connection_call($client, $method_name, $project_id, $asset_id, $thumbnail_size);
      $result = $this->convert_to_xml($result);
      dpm($result, "result");
    }
    catch (Exception $e) {
      dpm($e->getMessage, "message");
    }

    return $result;
  }
}

/**
 * Pulls assets from Shared Shelf by project
 */
class sharedshelf_fetcher_fetcher extends FeedsFetcher {

  private $fetch_options = array();

  private $thumbnail_sizes = array();

  function __construct($object) {
    // Call the base class to set the connection properties.
    parent::__construct($object);

    // setup the option arrays.
    $this->fetch_options = array(
      'projects' => t('List Projects'),
      'projects.assets' => t('List Assets for a Project'),
      'asset' => t('Asset'),
    );

    $this->thumbnail_sizes = array(
      '64' => '64',
      '128' => '128',
      '256' => '256',
      '512' => '512',
      '1024' => '1024',
      'full' => t('Full sized image'),
    );

  }

  /**
   * Implements FeedsFetcher::fetch().
   */
  public function fetch(FeedsSource $source) {
    $source_config = $source->getConfigFor($this);
    $client = $source_config['client'];
    $config = array('client' => $source_config['client'], 'settings' => $this->config);
    return new FeedsSharedshelfResult($config, $source->feed_nid);
  }

  /**
   * Override parent::configDefaults().
   */
  public function configDefaults() {
    return array(
      'operations' => 'projects',
      'project_id' => FALSE,
      'asset_id' => FALSE,
      'thumbnail_size' => '64',
    );
  }

  /**
   * Override parent::configForm().
   */
  public function configForm(&$form_state) {

    $form = array();
    // select the connection
    // select which function - list projects, project assets, etc.
    $form['operations'] = array(
      '#type' => 'select',
      '#key_type' => 'associative',
      '#title' => t('Select an operation'),
      '#description' => t('Choose what to fetch from ARTstor.'),
      '#options' => $this->fetch_options,
      '#default_value' => $this->config['operations'],
    );
    $form['project_id'] = array(
      '#type' => 'textfield',
      '#title' => t('Project ID'),
      '#description' => t('Enter the "adam_project_id" for the project.' . " Required for 'List Assets for a Project'."),
      '#default_value' => $this->config['project_id'],
    );
    $form['asset_id'] = array(
      '#type' => 'textfield',
      '#title' => t('Asset ID'),
      '#description' => t('Enter the Asset ID for the asset.' . " Required for 'Asset'."),
      '#default_value' => $this->config['asset_id'],
    );
    $form['thumbnail_size'] = array(
      '#type' => 'select',
      '#key_type' => 'associative',
      '#title' => t('Select an image size'),
      '#description' => t('Choose what sized image to fetch from ARTstor.' . " Required for 'Asset'."),
      '#options' => $this->thumbnail_sizes,
      '#default_value' => $this->config['thumbnail_size'],
    );
    return $form;
  }

  /**
   * Override parent::configFormValidate().
   */
  public function configFormValidate(&$values) {
    if ($values['operations'] == 'projects.assets' && empty($values['project_id'])) {
      $form_key = 'feeds][operations';
      form_set_error($form_key, t('Please enter a project id.'));
    }
    elseif ($values['operations'] == 'asset' && empty($values['asset_id'])) {
      $form_key = 'feeds][asset';
      form_set_error($form_key, t('Please enter an asset id.'));
    }
  }


  /**
   * Expose source form.
   */
  public function sourceForm($source_config) {
    // find existing clients (hoping one is an ARTstor)
    $connections = clients_connection_load_all();
    $dropdown_array = array('0' => '--none--');
    foreach ($connections as $name => $conn) {
      if ($conn->type == 'sharedshelf_client') {
        $key = $conn->cid;
        $val = $conn->name; // machine name
        $dropdown_array["$val"] = $val;
      }
    }
    $form = array();

    $form['client'] = array(
      '#type' => 'select',
      '#key_type' => 'associative',
      '#options' => $dropdown_array,
      '#title' => t('Select a client'),
      '#description' => t('If the right Sharedshelf client is not here add one at /admin/structure/clients.'),
      '#default_value' => isset($source_config['client']) ? $source_config['client'] : 0,
      '#required' => TRUE,
    );
    return $form;
  }

  /**
   * Override parent::sourceFormValidate().
   */
  public function sourceFormValidate(&$values) {
    if ($values['client'] == 0) {
      $form_key = 'feeds][client';
      //form_set_error($form_key, t('Please select a client'));
    }
  }

}
