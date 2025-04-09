function decodeUplink(input) {
    // Assurez-vous que le payload est une chaîne de texte
    let payload = String.fromCharCode.apply(null, input.bytes);
    
    // Séparer les valeurs par des virgules
    let values = payload.split(',');
  
    // Extraire chaque valeur et la convertir dans son type approprié
    let poids = parseFloat(values[0]);            // Poids avec 3 décimales
    let pourc_bat = parseFloat(values[1]);        // Batterie avec 1 décimale
    let temperature = parseFloat(values[2]);        // Température DHT11 (entier)
    let humidity = parseFloat(values[3]);           // Humidité DHT11 (entier)
    let ftemperature = parseFloat(values[4]);    // Température DHT22 (3 décimales)
    let fhumidity = parseFloat(values[5]);       // Humidité DHT22 (1 décimale)
    let tempOW = parseFloat(values[6]);           // Température OneWire (3 décimales)
    let lux = parseFloat(values[7]);              // LUX (3 décimales)
  
    // Retourner un objet avec les données extraites
    return {
      data: {
        key: "** secret key here **",
        weight_kg: poids,
        bv: pourc_bat,
        t: temperature,
        h: humidity,
        t_i: ftemperature,
        h_i: fhumidity,
        t_0: tempOW,
        l: lux
      },
      warnings: [],
      errors: []
    };
  }
  