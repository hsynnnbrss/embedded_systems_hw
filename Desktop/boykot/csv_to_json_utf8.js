const fs = require('fs');
const csv = require('csvtojson');

(async () => {
  const csvFilePath = './all_hadiths_clean.csv';
  const jsonFilePath = './all_hadiths_clean_utf8.json';

  // Dosyayı UTF-8 olarak oku
  const csvData = fs.readFileSync(csvFilePath, { encoding: 'utf8' });

  // Dönüştür
  const jsonArray = await csv().fromString(csvData);

  // JSON olarak kaydet
  fs.writeFileSync(jsonFilePath, JSON.stringify(jsonArray, null, 2), 'utf8');
  console.log('Dönüştürme tamamlandı: all_hadiths_clean_utf8.json');
})();